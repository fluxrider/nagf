// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac -classpath .. gfx-swing.java
// mkfifo gfx.fifo && LD_LIBRARY_PATH=libsrr java -cp .:servers -Djava.library.path=$(pwd)/libsrr gfx_swing && rm gfx.fifo

import java.io.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.util.*;
import java.util.List;
import java.util.stream.*;
import javax.swing.*;
import libsrr.*;
import java.nio.*;
import java.nio.file.*;
import java.util.concurrent.*;
import javax.imageio.*;

class gfx_swing {

  private static int W, H;
  private static boolean focused, quitting;
  private static BufferedImage backbuffer;
  private static BufferedImage frontbuffer;
  private static Graphics2D g;
  private static Graphics2D _g;

  // main
  public static void main(String[] args) throws Exception {
    String shm_path = args[0];

    // synchronization
    Semaphore should_quit = new Semaphore(0);
    Semaphore flush_pre = new Semaphore(0);
    Semaphore flush_post = new Semaphore(0);
    Semaphore queue_list = new Semaphore(0);

    // smooth scaling, smooth text
    Map<RenderingHints.Key, Object> hints;
    Map<RenderingHints.Key, Object> hints_low;
    hints = new HashMap<RenderingHints.Key, Object>();
    hints.put(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
    hints.put(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
    hints.put(RenderingHints.KEY_ALPHA_INTERPOLATION, RenderingHints.VALUE_ALPHA_INTERPOLATION_QUALITY);
    hints.put(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
    hints.put(RenderingHints.KEY_COLOR_RENDERING, RenderingHints.VALUE_COLOR_RENDER_QUALITY);
    hints.put(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
    hints_low = new HashMap<RenderingHints.Key, Object>();
    hints_low.put(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_NEAREST_NEIGHBOR);
    hints_low.put(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_OFF);
    hints_low.put(RenderingHints.KEY_ALPHA_INTERPOLATION, RenderingHints.VALUE_ALPHA_INTERPOLATION_SPEED);
    hints_low.put(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_OFF);
    hints_low.put(RenderingHints.KEY_COLOR_RENDERING, RenderingHints.VALUE_COLOR_RENDER_SPEED);
    hints_low.put(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_SPEED);

    // the panel scale-paints the front buffer on itself on each repaint call
    JPanel panel = new JPanel() {
      public void paint(Graphics g) {
        synchronized (frontbuffer) {
          int W = this.getWidth();
          int H = this.getHeight();
          int w = frontbuffer.getWidth();
          int h = frontbuffer.getHeight();
          // always smooth on minification, TODO let user decide if we should smooth scale on magnify too
          ((Graphics2D) g).addRenderingHints((W < w || H < h)? hints : hints_low);
          // respect aspect ratio (i.e. black bars)
          double a = w / (double) h;
          double A = W / (double) H;
          int offsetX = 0;
          int offsetY = 0;
          int aspectW = W;
          int aspectH = H;
          // top/down black bars
          if (a / A > 1) {
            aspectH = W * h / w;
            offsetY = (H - aspectH) / 2;
          }
          // left/right black bars
          else {
            aspectW = H * w / h;
            offsetX = (W - aspectW) / 2;
          }
          // draw front buffer on panel
          g.drawImage(frontbuffer, offsetX, offsetY, aspectW, aspectH, null);
        }
      }
    };
    JFrame frame = new JFrame();
    frame.setContentPane(panel);
    focused = false;
    quitting = false;
    frame.addWindowFocusListener(new WindowFocusListener() {
      public void windowLostFocus(WindowEvent e) {
        System.out.println(e);
        focused = false;
      }
      public void windowGainedFocus(WindowEvent e) {
        System.out.println(e);
        focused = true;
      }
    });
    frame.addWindowListener(new WindowListener() {
      public void windowActivated(WindowEvent e) {
        System.out.println(e);
      }
      public void windowClosed(WindowEvent e) {
        System.out.println(e);
      }
      public void windowClosing(WindowEvent e) {
        System.out.println(e);
        focused = false;
        quitting = true;
        should_quit.release();
      }
      public void windowDeactivated(WindowEvent e) {
        System.out.println(e);
      }
      public void windowDeiconified(WindowEvent e) {
        System.out.println(e);
      }
      public void windowIconified(WindowEvent e) {
        System.out.println(e);
      }
      public void windowOpened(WindowEvent e) {
        System.out.println(e);
      }
    });

    // resources
    Map<String, Object> cache = new TreeMap<>();

    // fifo thread (actual io)
    Queue<String> fifo_queue = new LinkedList<String>();
    Thread fifo_io = new Thread(new Runnable() {
      public void run() {
        try {
          // Note: sadly, java has no mkfifo at this time, and jna isn't standard lib, so here we assume the fifo already exists.
          // Simply put each command in a queue, to avoid blocking client
          while(true) {
            // note: Files.readAllLines lags with fifo, so I'm using the tried and true buffered reader instead.
            BufferedReader reader = new BufferedReader(new FileReader("gfx.fifo"));
            String line = reader.readLine();
            while (line != null) {
              synchronized(fifo_queue) { fifo_queue.add(line); }
              queue_list.release();
              // read next line
              line = reader.readLine();
            }
            reader.close();
          }
        } catch(Throwable t) {
          t.printStackTrace();
        } finally {
          should_quit.release();
        }
      }
    });
    fifo_io.start();

    // fifo thread (queue)
    Thread fifo = new Thread(new Runnable() {
      public void run() {
        try {
          while(true) {
            String command;
            queue_list.acquire();
            synchronized(fifo_queue) { command = fifo_queue.remove(); }
            if(command.equals("flush")) {
              // finish setting up window on first flush
              if(backbuffer == null) {
                if(W == 0) W = 800;
                if(H == 0) H = 450;
                panel.setPreferredSize(new Dimension(W, H));
                frame.pack();
                frame.setLocationRelativeTo(null);
    
                // create a backbuffer for drawing offline, and a front buffer to use when drawing the panel
                backbuffer = new BufferedImage(W, H, BufferedImage.TYPE_INT_ARGB);
                frontbuffer = new BufferedImage(W, H, BufferedImage.TYPE_INT_RGB);
                g = (Graphics2D) backbuffer.getGraphics(); g.addRenderingHints(hints);
                _g = (Graphics2D) frontbuffer.getGraphics(); _g.addRenderingHints(hints);

                // show window
                frame.setVisible(true);
              }

              // on flush, stop handling any more messages until srr thread completes the flush
              flush_pre.release();
              flush_post.acquire();
            } else if(command.startsWith("title ")) {
              System.out.println("setting title");
              frame.setTitle(command.substring(6));
            } else if(command.startsWith("window ")) {
              System.out.println("setting window size");
              String [] dim = command.substring(7).split(" ");
              W = Integer.parseInt(dim[0]);
              H = Integer.parseInt(dim[1]);
            } else if(command.startsWith("cache ")) {
              String path = command.substring(6);
              // fonts have sizes after the path
              String [] parts = path.split(" ");
              if(parts.length > 1) {
                System.out.println("caching a font: " + path);
                path = parts[0];
                Font font = Font.createFont(java.awt.Font.TRUETYPE_FONT, new File(path));
                Map<Float, Font> fonts = new TreeMap<>();
                for(int i = 1; i < parts.length; i++) {
                  Float size = Float.valueOf(parts[i]);
                  fonts.put(size, font.deriveFont(size));
                }
                cache.put(path, fonts);
              }
              // if not it's an image
              else {
                System.out.println("caching an image: " + path);
                try {cache.put(path, ImageIO.read(new File(path))); } catch(Exception e) { 
                  System.out.println("error caching image " + e);
                  cache.put(path, e);
                }
              }
            } else if(command.startsWith("draw ")) {
              String [] parts = command.split(" ");
              int i = 1;
              String path = parts[i++];
              if(parts.length == 4) {
                int x = (int)Double.parseDouble(parts[i++]);
                int y = (int)Double.parseDouble(parts[i++]);
                g.drawImage((BufferedImage)cache.get(path), x, y, null);
              } else {
                int sx = (int)Double.parseDouble(parts[i++]);
                int sy = (int)Double.parseDouble(parts[i++]);
                int w = (int)Double.parseDouble(parts[i++]);
                int h = (int)Double.parseDouble(parts[i++]);
                int x = (int)Double.parseDouble(parts[i++]);
                int y = (int)Double.parseDouble(parts[i++]);
                boolean mirror_x = Stream.of(parts).anyMatch(s -> s.equals("mx"));
                if(mirror_x) {
                  g.drawImage((BufferedImage)cache.get(path), x+w, y, x, y+h, sx, sy, sx+w, sy+h, null);
                } else {
                  g.drawImage((BufferedImage)cache.get(path), x, y, x+w, y+h, sx, sy, sx+w, sy+h, null);
                }
              }
            } else if(command.startsWith("text ")) {
              String [] parts = command.split(" ");
              String path = parts[1];
              Float size = Float.valueOf(parts[2]);
              double x = Double.parseDouble(parts[3]);
              double y = Double.parseDouble(parts[4]);
              Color fill = parse_color(parts[5]);
              Color outline = parse_color(parts[6]);
              // rebuild text (TODO this is getting messy)
              StringBuilder text = new StringBuilder();
              for(int i = 7; i < parts.length; i++) {
                text.append(parts[i]);
                text.append(' ');
              }
              g.setFont(((Map<Float, Font>)cache.get(path)).get(size));
              g.setColor(fill);
              // TODO outline  color
              g.drawString(text.toString(), (int)x, (int)y);
            }
          }
        } catch(Throwable t) {
          t.printStackTrace();
        } finally {
          should_quit.release();
        }
      }
    });
    fifo.start();

    // srr thread
    Thread srr = new Thread(new Runnable() {
      public void run() {
        try(srr srr = new srr(shm_path, 8192, true, false, 3)) {
          long t0 = System.currentTimeMillis();
          long delta_time = 0;
          while(true) {
            // sync with client
            String [] sync = srr.as_string(srr.receive()).toString().split(" ");
            srr.msg.position(0);
            srr.msg.put(focused? (byte)1 : (byte)0);
            srr.msg.put(quitting? (byte)1 : (byte)0);
            srr.msg.putInt(W);
            srr.msg.putInt(H);
            int i = 0;
            while(i < sync.length) {
              String command = sync[i++];
              if(command.equals("flush")) {
                // let fifo drain until flush to ensure all drawing have been done
                flush_pre.acquire();
                // flush our drawing to the screen
                if(!quitting) {
                  synchronized (frontbuffer) { _g.drawImage(backbuffer, 0, 0, null); }
                  panel.repaint();
                  Thread.yield();
                }
                // delta_time
                long t1 = System.currentTimeMillis();
                delta_time = t1 - t0;
                t0 = t1;
                // let fifo resume
                flush_post.release();
              } else if(command.equals("delta")) {
                srr.msg.put((byte)3);
                srr.msg.putInt((int)delta_time);
              } else if(command.equals("stat")) {
                String path = sync[i++];
                Object res = cache.get(path);
                if(res == null) {
                  // drain fifo (but not past a flush) and try again.
                  // ideally, user would always flush before doing stats
                  // however, on first frame, this scenario is legit,
                  // so before returning an error, we will give the fifo thread a chance,
                  // but we'll print a warning because of the horrid lag.
                  long max_wait = 500;
                  while(max_wait > 0 && flush_post.availablePermits() == 0) {
                    System.out.println("Warning: waiting for fifo to drain for stat.");
                    Thread.sleep(10);
                    res = cache.get(path);
                    if(res != null) break;
                    max_wait -= 10;
                  }
                  if(max_wait <= 0) System.out.println("Warning: reached max waiting period");
                  else System.out.println("Warning: had to wait " + (500 - max_wait) + " ms.");
                }
                if(res == null) { res = new RuntimeException("unknown path"); }
                if(res instanceof Exception) {
                  srr.msg.put((byte)0);
                  if(res instanceof FileNotFoundException) {
                    srr.msg.put((byte)(int)'F'); 
                    srr.msg.put((byte)(int)'N'); 
                    srr.msg.put((byte)(int)'F'); 
                  } else if(res instanceof IOException) {
                    srr.msg.put((byte)(int)'I'); 
                    srr.msg.put((byte)(int)'O'); 
                    srr.msg.put((byte)(int)' '); 
                  } else {
                    srr.msg.put((byte)(int)'E'); 
                    srr.msg.put((byte)(int)' '); 
                    srr.msg.put((byte)(int)' '); 
                  }
                } else if(res instanceof BufferedImage) {
                  srr.msg.put((byte)1);
                  BufferedImage image = (BufferedImage)res;
                  srr.msg.putInt(image.getWidth());
                  srr.msg.putInt(image.getHeight());
                } else if(res instanceof Map) {
                  srr.msg.put((byte)2);
                  // new Canvas().getFontMetrics(font);
                } else if(res instanceof Progress) {
                  Progress p = (Progress)res;
                  srr.msg.put(p.what);
                  srr.msg.putInt(0);
                  srr.msg.putInt((int)(p.value * 1000));
                }
              } else {
                throw new RuntimeException("unknown command: " + command);
              }
            }
            srr.reply(srr.msg.position());
          }
        } catch(Throwable t) {
          t.printStackTrace();
        } finally {
          should_quit.release();
        }
      }
    });
    srr.start();
    
    should_quit.acquire();
    if(quitting) {
      // give ourself a second to report that we are quitting to the client
      try { Thread.sleep(1000); } finally { }
    }
    System.exit(0);
  }

  private class Progress {
    public byte what;
    public double value;
  }
  
  private static Color parse_color(String hex) {
    // from https://stackoverflow.com/questions/4129666/how-to-convert-hex-to-rgb-using-java, Ian Newland
    switch(hex.length()) {
      case 6:
        return new Color(
        Integer.valueOf(hex.substring(0, 2), 16),
        Integer.valueOf(hex.substring(2, 4), 16),
        Integer.valueOf(hex.substring(4, 6), 16));
      case 8:
        return new Color(
        Integer.valueOf(hex.substring(0, 2), 16),
        Integer.valueOf(hex.substring(2, 4), 16),
        Integer.valueOf(hex.substring(4, 6), 16),
        Integer.valueOf(hex.substring(6, 8), 16));
    }
    return null;
  }
}
