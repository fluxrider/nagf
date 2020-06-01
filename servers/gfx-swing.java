// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac -classpath .. gfx-swing.java
// LD_LIBRARY_PATH=libsrr java -cp .:servers -Djava.library.path=$(pwd)/libsrr gfx_swing

import java.io.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.util.*;
import java.util.List;
import javax.swing.*;
import libsrr.*;

class gfx_swing {

/*
TMP
    return javax.imageio.ImageIO.read(new File(path));
    Font font = Font.createFont(java.awt.Font.TRUETYPE_FONT, new File(path));
    font = font.deriveFont((float) size);
    return new Canvas().getFontMetrics(font);
*/

  // main
  public static void main(String[] args) {
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
    int W = 800;
    int H = 450;
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
    frame.setContentPane(panel);
    frame.setVisible(true);
    frame.addWindowFocusListener(new WindowFocusListener() {
      public void windowLostFocus(WindowEvent e) {
        System.out.println(e);
      }
      public void windowGainedFocus(WindowEvent e) {
        System.out.println(e);
      }
    });

    // resources
    Map<String, Object> cache = new TreeMap<>();

    // synchronization
    Object should_quit = new Object();

    // fifo thread (actual io)
    Thread fifo_io = new Thread(new Runnable() {
      public void run() {
        try {
          // simply put each command in a queue, to avoid blocking client
        } catch(Throwable t) {
          t.printStackTrace();
        } finally {
          synchronized(should_quit) { should_quit.notify(); }
        }
      }
    });
    fifo_io.start();

    // fifo thread (queue)
    Thread fifo = new Thread(new Runnable() {
      public void run() {
        try {
          // TODO if command is 'flush', stop handling any more messages until srr thread lets me
        } catch(Throwable t) {
          t.printStackTrace();
        } finally {
          synchronized(should_quit) { should_quit.notify(); }
        }
      }
    });
    fifo.start();

    // srr thread
    Thread srr = new Thread(new Runnable() {
      public void run() {
        try(srr srr = new srr("/gfx-swing", 8192, true, false, 3)) {
          long t0 = System.currentTimeMillis();
          while(true) {
            // sync with client
            String [] sync = srr.as_string(srr.receive()).split(" ");
            srr.msg.position(0);
            while(i < sync.length) {
              String command = sync[i++];
              if(command.equals("flush")) {
                // TODO drain fifo till flush to ensure all drawing have been done
                // flush our drawing to the screen
                synchronized (frontbuffer) {
                  _g.drawImage(backbuffer, 0, 0, null);
                }
                panel.repaint();
                Thread.yield();
                // clear backbuffer
                g_clear.fillRect(0, 0, W, H);
                // fps
                long t1 = System.currentTimeMillis();
                double fps = 1000.0 / (t1 - t0);
                t0 = t1;
                // let fifo resume
              } else if(command.equals("fps")) {
                srr.msg.putInt(3);
                srr.msg.putInt(fps * 1000);
              } else if(command.equals("stat")) {
                // TODO drain fifo (but not past flush) to ensure path is understood
                String path = sync[i++];
                Object res = cache.get(path);
                if(res == null) res = new RuntimeException("unknown path");
                if(res instanceof Exception) {
                  srr.msg.putInt(0);
                  if(res instanceof FileNotFoundException) {
                    srr.msg.put(Character.getNumericalValue('F')); 
                    srr.msg.put(Character.getNumericalValue('N')); 
                    srr.msg.put(Character.getNumericalValue('F')); 
                  } else if(res instanceof IOException) {
                    srr.msg.put(Character.getNumericalValue('I')); 
                    srr.msg.put(Character.getNumericalValue('O')); 
                    srr.msg.put(Character.getNumericalValue(' ')); 
                  } else if(res instanceof Exception) {
                    srr.msg.put(Character.getNumericalValue('E')); 
                    srr.msg.put(Character.getNumericalValue(' ')); 
                    srr.msg.put(Character.getNumericalValue(' ')); 
                  }
                } else if(res instanceof BufferedImage) {
                  srr.msg.putInt(1);
                  BufferedImage image = (BufferedImage)res;
                  srr.msg.putInt(image.getWidth());
                  srr.msg.putInt(image.getHeight());
                } else if(res instanceof Font) {
                  srr.msg.putInt(2);
                } else if(res instanceof Progress) {
                  Progress p = (Progress)res;
                  srr.msg.putInt(p.what);
                  srr.msg.putInt(0);
                  srr.msg.putInt(p.value * 1000);
                }
              } else {
                throw new RuntimeException("unknown command: " + command);
              }
            }
            srr.reply(0);
          }
        } catch(Throwable t) {
          t.printStackTrace();
        } finally {
          synchronized(should_quit) { should_quit.notify(); }
        }
      }
    });
    srr.start();
    
    synchronized(should_quit) { should_quit.wait(); }
    System.exit(0);
  }

}
