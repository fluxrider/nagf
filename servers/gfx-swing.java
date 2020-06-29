// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later.
// javac -classpath .. gfx-swing.java
// mkfifo gfx.fifo && LD_LIBRARY_PATH=libsrr java -cp .:servers -Djava.library.path=$(pwd)/libsrr gfx_swing && rm gfx.fifo

import java.io.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.awt.font.*;
import java.awt.geom.*;
import java.util.*;
import java.util.List;
import java.util.stream.*;
import javax.swing.*;
import libsrr.*;
import java.nio.*;
import java.nio.file.*;
import java.util.concurrent.*;
import javax.imageio.*;
import javax.imageio.event.*;
import javax.imageio.stream.*;

class gfx_swing {

  private static int W, H;
  private static int preferred_W, preferred_H;
  private static double _scale = 1;
  private static boolean hq;
  private static boolean focused, quitting;
  private static BufferedImage backbuffer;
  private static BufferedImage frontbuffer;
  private static Graphics2D g;
  private static Graphics2D _g;

  public static byte GFX_STAT_ERR = 2;
  public static byte GFX_STAT_IMG = 1;
  public static byte GFX_STAT_FNT = 2;
  public static byte GFX_STAT_DLT = 3;
  
  // main
  public static void main(String[] args) throws Exception {
    String shm_path = args[0];

    // synchronization
    Semaphore should_quit = new Semaphore(0);
    Semaphore flush_pre = new Semaphore(0);
    Semaphore flush_post = new Semaphore(0);
    Semaphore queue_list = new Semaphore(0);
    Object frontbuffer_mutex = new Object();
    Object backbuffer_mutex = new Object();

    // smooth scaling, smooth text
    Map<RenderingHints.Key, Object> hints_g;
    Map<RenderingHints.Key, Object> hints;
    Map<RenderingHints.Key, Object> hints_low;
    hints_g = new HashMap<RenderingHints.Key, Object>();
    hints_g.put(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_NEAREST_NEIGHBOR);
    hints_g.put(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
    hints_g.put(RenderingHints.KEY_ALPHA_INTERPOLATION, RenderingHints.VALUE_ALPHA_INTERPOLATION_QUALITY);
    hints_g.put(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
    hints_g.put(RenderingHints.KEY_COLOR_RENDERING, RenderingHints.VALUE_COLOR_RENDER_QUALITY);
    hints_g.put(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
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
        synchronized (frontbuffer_mutex) {
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

    // recreate a backbuffer/frontbuffer on resize when in hq mode, not that the current frame will be corrupted TODO resize after flush
    panel.addComponentListener(new ComponentAdapter() {
      public void componentResized(ComponentEvent e) {
        if(!hq) return;
        if(backbuffer == null) throw new RuntimeException("didn't expect a resize before first flush");
        System.out.println(panel.getWidth() + "x" + panel.getHeight());
        // get new dimension, though respect aspect ratio of logical size
        int pw = panel.getWidth();
        int ph = panel.getHeight();
        double a = W / (double) H;
        double A = pw / (double) ph;
        int _W = pw;
        int _H = ph;
        if (a / A > 1) _H = pw * H / W; else _W = ph * W / H;
        if(_W < 1) _W = 1;
        if(_H < 1) _H = 1;
        synchronized(backbuffer_mutex) {
          backbuffer = new BufferedImage(_W, _H, BufferedImage.TYPE_INT_ARGB);
          g = (Graphics2D) backbuffer.getGraphics(); g.addRenderingHints(hints_g);
          _scale = _W / (double)W;
          g.scale(_scale, _scale);
          synchronized(frontbuffer_mutex) {
            frontbuffer = new BufferedImage(_W, _H, BufferedImage.TYPE_INT_RGB);
            _g = (Graphics2D) frontbuffer.getGraphics(); _g.addRenderingHints(hints);
          }
        }
      }
    });

    // resources
    Map<String, Object> cache = Collections.synchronizedSortedMap(new TreeMap<>());

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
          AffineTransform stored_at = null;
          while(true) {
            String command;
            queue_list.acquire();
            synchronized(fifo_queue) { command = fifo_queue.remove(); }
            if(command.equals("flush")) {
              // finish setting up window on first flush
              if(!frame.isVisible()) {
                // show window
                boolean no_pref = preferred_W == 0;
                if(no_pref) {
                  preferred_W = W;
                  preferred_H = H;
                }
                panel.setPreferredSize(new Dimension(preferred_W, preferred_H));
                frame.pack();
                frame.setLocationRelativeTo(null);
                if(no_pref) frame.setExtendedState(JFrame.MAXIMIZED_BOTH); 
                frame.setVisible(true);
              }

              // on flush, stop handling any more messages until srr thread completes the flush
              flush_pre.release();
              flush_post.acquire();
            } else if(command.startsWith("title ")) {
              System.out.println("setting title");
              frame.setTitle(command.substring(6));
            } else if(command.equals("hq")) {
              System.out.println("hq mode activated");
              hq = true;
            } else if(command.startsWith("window ")) {
              System.out.println("setting window size");
              String [] dim = command.substring(7).split(" ");
              if(backbuffer != null) throw new RuntimeException("there is no support for changing logical size after the fact");
              W = Integer.parseInt(dim[0]);
              H = Integer.parseInt(dim[1]);
              if(dim.length > 2) {
                preferred_W = Integer.parseInt(dim[2]);
                preferred_H = Integer.parseInt(dim[3]);
              }
              // create a backbuffer for drawing offline, and a front buffer to use when drawing the panel
              backbuffer = new BufferedImage(W, H, BufferedImage.TYPE_INT_ARGB);
              frontbuffer = new BufferedImage(W, H, BufferedImage.TYPE_INT_RGB);
              g = (Graphics2D) backbuffer.getGraphics(); g.addRenderingHints(hints_g);
              _g = (Graphics2D) frontbuffer.getGraphics(); _g.addRenderingHints(hints);
            } else if(command.startsWith("scale ")) {
              String [] parts = command.split(" ");
              double sx = Double.parseDouble(parts[1]);
              double sy = 2 < parts.length? Double.parseDouble(parts[2]) : sx;
              if(stored_at != null) throw new RuntimeException("scale isn't a stack, there is only one");
              synchronized(backbuffer_mutex) {
                // NOTE: this breaks if there is a hq resize before unscale... not sure if I want to support scaling anyway
                stored_at = g.getTransform();
                g.scale(sx, sy);
              }
            } else if(command.equals("unscale")) {
              synchronized(backbuffer_mutex) {
                g.setTransform(stored_at);
              }
              stored_at = null;
            } else if(command.startsWith("cache ")) {
              String path = command.substring(6);
              if(cache.get(path) != null) System.out.println("GFX cache refresh " + path); else {
                if(path.endsWith(".ttf")) {
                  cache.put(path, new Progress(GFX_STAT_FNT));
                  new Thread(new Runnable() {
                    public void run() {
                      try {
                        cache.put(path, Font.createFont(java.awt.Font.TRUETYPE_FONT, new File(path)));
                      } catch(Exception e) { 
                        System.out.println("GFX error caching font " + e);
                        cache.put(path, e);
                      }
                    }
                  }).start();
                }
                // anything but .ttf goes through ImageIO
                else {
                  Progress p = new Progress(GFX_STAT_IMG);
                  cache.put(path, p);
                  new Thread(new Runnable() {
                    public void run() {
                      try {
                        ImageInputStream stream = ImageIO.createImageInputStream(new File(path));
                        Iterator<ImageReader> readers_itr = ImageIO.getImageReaders​(stream);
                        if(!readers_itr.hasNext()) cache.put(path, new RuntimeException("unsupported image format"));
                        Exception error = null;
                        while(readers_itr.hasNext()) {
                          try {
                            ImageReader reader = readers_itr.next();
                            reader.setInput(stream);
                            IIOReadProgressListener listener = new IIOReadProgressListener() {
                              public void imageComplete(ImageReader source) { }
                              public void imageProgress(ImageReader source, float percentageDone) { p.value = percentageDone; }
                              public void imageStarted(ImageReader source, int imageIndex) { }
                              public void readAborted(ImageReader source) { }
                              public void sequenceComplete(ImageReader source) { }
                              public void sequenceStarted(ImageReader source, int minIndex) { }
                              public void thumbnailComplete(ImageReader source) { }
                              public void thumbnailProgress(ImageReader source, float percentageDone) { }
                              public void thumbnailStarted(ImageReader source, int imageIndex, int thumbnailIndex) { }
                            };
                            reader.addIIOReadProgressListener​(listener);
                            cache.put(path, reader.read(0));
                            reader.removeIIOReadProgressListener​(listener);
                            error = null;
                          } catch(Exception e) {
                            System.out.println("GFX error caching image " + e + " try again? " + readers_itr.hasNext());
                            if(error == null) error = e;
                          }
                          if(error == null) break;
                        }
                      } catch(Exception e) { 
                        System.out.println("GFX general error caching image " + e);
                        cache.put(path, e);
                      }
                    }
                  }).start();
                }
              }
            } else if(command.startsWith("draw ")) {
              String [] parts = command.split(" ");
              int i = 1;
              String path = parts[i++];
              Object c = cache.get(path);
              if(c == null) System.out.println("GFX error: null image resource for " + path); else
              if(!(c instanceof BufferedImage)) System.out.println("GFX error: no image resource ready for " + path); else {
                BufferedImage image = (BufferedImage)c;
                switch(parts.length) {
                  // normal: draw path x y
                  case 4:
                  {
                    int x = (int)Double.parseDouble(parts[i++]);
                    int y = (int)Double.parseDouble(parts[i++]);
                    synchronized(backbuffer_mutex) {
                      g.drawImage(image, x, y, null);
                    }
                  }
                  break;
                  // scaled: draw path x y w h
                  case 6:
                  {
                    int x = (int)Double.parseDouble(parts[i++]);
                    int y = (int)Double.parseDouble(parts[i++]);
                    int w = (int)Double.parseDouble(parts[i++]);
                    int h = (int)Double.parseDouble(parts[i++]);
                    synchronized(backbuffer_mutex) {
                      g.drawImage(image, x, y, w, h, null);
                    }
                  }
                  break;
                  // region: draw path sx sy w h x y (mx=mirror-x)
                  case 8:
                  case 9:
                  {
                    int sx = (int)Double.parseDouble(parts[i++]);
                    int sy = (int)Double.parseDouble(parts[i++]);
                    int w = (int)Double.parseDouble(parts[i++]);
                    int h = (int)Double.parseDouble(parts[i++]);
                    int x = (int)Double.parseDouble(parts[i++]);
                    int y = (int)Double.parseDouble(parts[i++]);
                    boolean mirror_x = Stream.of(parts).anyMatch(s -> s.equals("mx"));
                    synchronized(backbuffer_mutex) {
                      if(mirror_x) {
                        g.drawImage(image, x+w, y, x, y+h, sx, sy, sx+w, sy+h, null);
                      } else {
                        g.drawImage(image, x, y, x+w, y+h, sx, sy, sx+w, sy+h, null);
                      }
                    }
                  }
                  break;
                  // region: draw path sx sy sw sh x y w h
                  case 10:
                  {
                    int sx = (int)Double.parseDouble(parts[i++]);
                    int sy = (int)Double.parseDouble(parts[i++]);
                    int sw = (int)Double.parseDouble(parts[i++]);
                    int sh = (int)Double.parseDouble(parts[i++]);
                    int x = (int)Double.parseDouble(parts[i++]);
                    int y = (int)Double.parseDouble(parts[i++]);
                    int w = (int)Double.parseDouble(parts[i++]);
                    int h = (int)Double.parseDouble(parts[i++]);
                    synchronized(backbuffer_mutex) {
                      g.drawImage(image, x, y, x+w, y+h, sx, sy, sx+sw, sy+sh, null);
                    }
                  }
                  break;
                  default:
                    System.out.println("GFX error: bad draw command " + command);
                }
              }
            } else if(command.startsWith("text ")) {
              // text font x y w h valign halign line_count clip scroll outline_color fill_color message
              String [] parts = command.split(" ");
              int i = 1;
              String path = parts[i++];
              boolean tight = parts[i].equals("tight");
              if(tight) i++;
              double x = Double.parseDouble(parts[i++]);
              double y = Double.parseDouble(parts[i++]);
              double w = Double.parseDouble(parts[i++]);
              double h = Double.parseDouble(parts[i++]);
              String valign = parts[i++];
              String halign = parts[i++];
              int line_count = Integer.parseInt(parts[i++]);
              double line_height = h / line_count;
              boolean do_clip = parts[i++].equals("clip");
              double scroll = Double.parseDouble(parts[i++]);
              Color outline = parse_color(parts[i++]);
              Color fill = parse_color(parts[i++]);
              double outline_size = Double.parseDouble(parts[i++]) / _scale;
              // rebuild text (TODO this is getting messy)
              StringBuilder text = new StringBuilder();
              while(i < parts.length) {
                text.append(parts[i++]);
                text.append(' ');
              }
              text.setLength​(text.length() - 1);

              synchronized(backbuffer_mutex) {
                // get font size
                FontRenderContext frc = g.getFontRenderContext();
                Object c = cache.get(path);
                if(c == null) System.out.println("GFX error: null font resource for " + path); else
                if(!(c instanceof Font)) System.out.println("GFX error: no font resource ready for " + path); else {
                  Font font = (Font)c;
                  font = font.deriveFont((float)(line_height * (tight? 1.29 : 1))); // TODO loop instead of magic number that probably doesn't work?

                  // break explicit \n into multiple lines
                  String t = text.toString();
                  List<String> lines = new ArrayList<>();
                  int escape = t.indexOf​("\\n");
                  while(escape != -1) {
                    lines.add(text.substring(0, escape));
                    t = text.substring(escape + 2);
                    escape = t.indexOf​("\\n");
                  }
                  lines.add(t);

                  // text to glyph, break lines and repeat
                  ListIterator<String> itr = lines.listIterator();
                  List<GlyphVector> glyphs = new ArrayList<>();
                  while(itr.hasNext()) {
                    String line = itr.next();
                    int end = line.length();
                    GlyphVector gv = font.createGlyphVector(frc, line);
                    Rectangle2D box = gv.getVisualBounds();
                    while(box.getWidth() > w) {
                      // move to previous space
                      int new_end = line.substring(0, end).lastIndexOf(' ');
                      if(new_end == -1) break; // we did our best
                      end = new_end;
                      gv = font.createGlyphVector(frc, line.substring(0, end));
                      box = gv.getVisualBounds();
                    }
                    glyphs.add(gv);
                    if(end != line.length()) {
                      itr.add(line.substring(end+1));
                      itr.previous();
                    }
                  }

                  // render with outline
                  Shape clip = null;
                  if(do_clip) {
                    clip = g.getClip();
                    g.clipRect((int)x,(int)y,(int)w,(int)h);
                  }
                  double descent = tight? 1 : font.getLineMetrics("tj", frc).getDescent();
                  y += scroll;
                  y += outline_size;
                  if(valign.equals("bottom")) y += h - glyphs.size() * line_height;
                  else if(valign.equals("center")) y += (h - glyphs.size() * line_height) / 2;
                  for(GlyphVector gv : glyphs) {
                    Rectangle2D box = gv.getVisualBounds();
                    double tx = x - box.getX() + outline_size;
                    if(halign.equals("right")) tx += w - box.getWidth() - 1 - 2 * outline_size;
                    else if(halign.equals("center")) tx += (w - box.getWidth() - 1 - outline_size) / 2;
                    Shape shape = gv.getOutline((float)tx, (float)(y + line_height - descent));
                    g.setColor(fill);
                    g.fill(shape);
                    if(outline_size > 0) {
                      g.setStroke(new BasicStroke((float)outline_size));
                      g.setColor(outline);
                      g.draw(shape);
                    }
                    y += line_height;
                  }
                  if(do_clip) {
                    g.setClip(clip);
                  }
                }
              }
            } else if(command.startsWith("fill ")) {
              String [] parts = command.split(" ");
              Color fill = parse_color(parts[1]);
              double x = Double.parseDouble(parts[2]);
              double y = Double.parseDouble(parts[3]);
              double w = Double.parseDouble(parts[4]);
              double h = Double.parseDouble(parts[5]);
              synchronized(backbuffer_mutex) {
                g.setColor(fill);
                g.fillRect((int)x, (int)y, (int)w, (int)h);
              }
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
                  synchronized(backbuffer_mutex) {
                    synchronized(frontbuffer_mutex) { _g.drawImage(backbuffer, 0, 0, null); }
                  }
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
                srr.msg.put(GFX_STAT_DLT);
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
                    System.out.println("GFX Warning: waiting for fifo to drain for stat.");
                    Thread.sleep(10);
                    res = cache.get(path);
                    if(res != null) break;
                    max_wait -= 10;
                  }
                  if(max_wait <= 0) System.out.println("GFX Warning: reached max waiting period");
                  else System.out.println("GFX Warning: had to wait " + (500 - max_wait) + " ms.");
                }
                if(res == null) { res = new RuntimeException("unknown resource"); }
                if(res instanceof Exception) {
                  srr.msg.put(GFX_STAT_ERR);
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
                  srr.msg.put(GFX_STAT_IMG);
                  BufferedImage image = (BufferedImage)res;
                  srr.msg.putInt(image.getWidth());
                  srr.msg.putInt(image.getHeight());
                } else if(res instanceof Font) {
                  srr.msg.put(GFX_STAT_FNT);
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

  private static class Progress {
    public byte what;
    public double value;
    public Progress(int what) {
      this.what = (byte)what;
    }
  }
  
  private static Color parse_color(String hex) {
    // inspired by https://stackoverflow.com/questions/4129666/how-to-convert-hex-to-rgb-using-java, Ian Newland
    switch(hex.length()) {
      case 6:
        return new Color(
        Integer.valueOf(hex.substring(0, 2), 16),
        Integer.valueOf(hex.substring(2, 4), 16),
        Integer.valueOf(hex.substring(4, 6), 16));
      case 8:
        return new Color(
        Integer.valueOf(hex.substring(2, 4), 16),
        Integer.valueOf(hex.substring(4, 6), 16),
        Integer.valueOf(hex.substring(6, 8), 16),
        Integer.valueOf(hex.substring(0, 2), 16));
    }
    return null;
  }
}
