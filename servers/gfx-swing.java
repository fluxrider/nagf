// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac gfx-swing
// java gfx_swing

import java.io.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.util.*;
import java.util.List;
import javax.swing.*;

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
    JFrame frame = new JFrame("");
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
    frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE); // TODO proper burial
    frame.addWindowFocusListener(new WindowFocusListener() {
      public void windowLostFocus(WindowEvent e) {
        System.out.println(e);
      }
      public void windowGainedFocus(WindowEvent e) {
        System.out.println(e);
      }
    });

    // game loop
    long t0 = System.currentTimeMillis();
    boolean running = true;
    int frame_count = 0;
    while (running) {
      // time flow
      long t1 = System.currentTimeMillis();
      double delta_time = (t1 - t0) / 1000.0;
      t0 = t1;
      // fps
      frame_count++;
      //System.out.println(frame_count + ": " + 1 / delta_time);

      // TODO sync with client

      // flush our drawing to the screen
      synchronized (frontbuffer) {
        _g.drawImage(backbuffer, 0, 0, null);
      }
      panel.repaint();
      Thread.yield();
      // clear backbuffer
      g_clear.fillRect(0, 0, W, H);
    }

    // if the loop ended, close the canvas
    frame.dispatchEvent(new WindowEvent(frame, WindowEvent.WINDOW_CLOSING));
  }

}
