// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac -classpath .. gfx-swing.java
// mkfifo gfx-swing.fifo && LD_LIBRARY_PATH=libsrr java -cp .:servers -Djava.library.path=$(pwd)/libsrr gfx_swing && rm gfx-swing.fifo

import java.io.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.util.*;
import java.util.List;
import javax.swing.*;
import libsrr.*;
import java.nio.*;
import java.nio.file.*;
import java.util.concurrent.*;
import javax.imageio.*;

class gfx_swing {

  private static int W, H;
  private static boolean focused;

  // main
  public static void main(String[] args) throws Exception {
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

    // create window centered in screen
    W = 800;
    H = 450;
    JFrame frame = new JFrame();
    frame.setSize(W, H);
    frame.setLocationRelativeTo(null);
    
    // create a backbuffer for drawing offline, and a front buffer to use when drawing the panel
    BufferedImage backbuffer = new BufferedImage(W, H, BufferedImage.TYPE_INT_ARGB);
    BufferedImage frontbuffer = new BufferedImage(W, H, BufferedImage.TYPE_INT_RGB);
    Graphics2D g = (Graphics2D) backbuffer.getGraphics(); g.addRenderingHints(hints);
    Graphics2D _g = (Graphics2D) frontbuffer.getGraphics(); _g.addRenderingHints(hints);
    Graphics2D g_clear = (Graphics2D) backbuffer.getGraphics();
    g_clear.setComposite(AlphaComposite.getInstance(AlphaComposite.CLEAR, 0.0f));
    g_clear.setColor(new Color(0, 0, 0, 0));

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

    // show window TODO wait for first frame and title before doing this
    focused = false;
    frame.setContentPane(panel);
    frame.setVisible(true);
    frame.addWindowFocusListener(new WindowFocusListener() {
      public void windowLostFocus(WindowEvent e) {
        System.out.println(e);
        focused = true;
      }
      public void windowGainedFocus(WindowEvent e) {
        System.out.println(e);
        focused = false;
      }
    });

    // resources
    Map<String, Object> cache = new TreeMap<>();

    // synchronization
    Semaphore should_quit = new Semaphore(0);
    Semaphore flush_pre = new Semaphore(0);
    Semaphore flush_post = new Semaphore(0);
    Semaphore queue_list = new Semaphore(0);

    // fifo thread (actual io)
    Queue<String> fifo_queue = new LinkedList<String>();
    Thread fifo_io = new Thread(new Runnable() {
      public void run() {
        try {
          // Note: sadly, java has no mkfifo at this time, and jna isn't standard lib, so here we assume the fifo already exists.
          // Simply put each command in a queue, to avoid blocking client
          while(true) {
            /*
            List<String> lines = Files.readAllLines(Paths.get("gfx-swing.fifo"));
            System.out.println("fifo read " + lines.size() + " lines");
            synchronized(fifo_queue) {
              fifo_queue.addAll(lines);
            }
            queue_list.release(lines.size());
            */
            BufferedReader reader = new BufferedReader(new FileReader("gfx-swing.fifo"));
            String line = reader.readLine();
            while (line != null) {
              System.out.println("fifo read 1 line");
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
            System.out.println("Handling fifo command: " + command);
            if(command.equals("flush")) {
              System.out.println("fifo flush");
              // on flush, stop handling any more messages until srr thread completes the flush
              flush_pre.release();
              flush_post.acquire();
              System.out.println("fifo flush done");
            } else if(command.startsWith("title ")) {
              System.out.println("setting title");
              frame.setTitle(command.substring(6));
            } else if(command.startsWith("cache ")) {
              String path = command.substring(6);
              // fonts have sizes after the path
              String [] parts = path.split(" ");
              if(parts.length > 1) {
                System.out.println("caching a font");
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
                System.out.println("caching an image");
                try {cache.put(path, ImageIO.read(new File(path))); } catch(Exception e) { 
                  System.out.println("error caching image " + e);
                  cache.put(path, e);
                }
              }
            } else if(command.startsWith("draw ")) {
              String [] parts = command.split(" ");
              String path = parts[1];
              double x = Double.parseDouble(parts[2]);
              double y = Double.parseDouble(parts[3]);
              g.drawImage((BufferedImage)cache.get(path), (int)x, (int)y, null);
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
        try(srr srr = new srr("/gfx-swing", 8192, true, false, 3)) {
          long t0 = System.currentTimeMillis();
          double fps = 0;
          while(true) {
            // sync with client
            String [] sync = srr.as_string(srr.receive()).toString().split(" ");
            srr.msg.position(0);
            srr.msg.put(focused? (byte)1 : (byte)0);
            srr.msg.putInt(W);
            srr.msg.putInt(H);
            int i = 0;
            while(i < sync.length) {
              String command = sync[i++];
              if(command.equals("flush")) {
                System.out.println("sync flush");
                // let fifo drain until flush to ensure all drawing have been done
                flush_pre.acquire();
                // flush our drawing to the screen
                synchronized (frontbuffer) { _g.drawImage(backbuffer, 0, 0, null); }
                panel.repaint();
                Thread.yield();
                // clear backbuffer
                g_clear.fillRect(0, 0, W, H);
                // TMP why isn't g_clear working?
                g.setColor(Color.WHITE);
                g.fillRect(0, 0, W, H);
                // fps
                long t1 = System.currentTimeMillis();
                fps = 1000.0 / (t1 - t0);
                t0 = t1;
                // let fifo resume
                flush_post.release();
                System.out.println("sync flush done");
              } else if(command.equals("fps")) {
                System.out.println("sync fps");
                srr.msg.put((byte)3);
                srr.msg.putInt((int)(fps * 1000));
              } else if(command.equals("stat")) {
                System.out.println("sync stat");
                String path = sync[i++];
                System.out.println("stat path: " + path);
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
                  System.out.println("stat error " + res);
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
                  System.out.println("stat img");
                  srr.msg.put((byte)1);
                  BufferedImage image = (BufferedImage)res;
                  srr.msg.putInt(image.getWidth());
                  srr.msg.putInt(image.getHeight());
                } else if(res instanceof Map) {
                  System.out.println("stat font");
                  srr.msg.put((byte)2);
                  // new Canvas().getFontMetrics(font);
                } else if(res instanceof Progress) {
                  System.out.println("stat progress");
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
