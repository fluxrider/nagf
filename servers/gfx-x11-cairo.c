// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o gfx-x11-cairo gfx-x11-cairo.c -Wall $(pkg-config --libs --cflags x11 cairo) -lpthread
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <cairo-xlib.h>

uint64_t currentTimeMillis() {
  struct timespec tp;
  if(clock_gettime(CLOCK_MONOTONIC, &tp) == -1) { perror("read"); exit(1); }
  return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

int x_error_handler(Display * d, XErrorEvent * e) {
  printf("gfx-x11-cairo x_error_handler: %d\n", e->error_code);
  exit(EXIT_FAILURE);
}

bool starts_with(const char * s, const char * start) {
  return strncmp(start, s, strlen(start)) == 0;
}

struct shared_amongst_thread_t {
  bool running;
  const char * server_fifo_path;
  Display * display;
  Drawable window;
};

void * handle_fifo_loop(void * vargp) {
  struct shared_amongst_thread_t * t = vargp;
  while(t->running) {
    FILE * f = fopen(t->server_fifo_path, "r"); if(!f) { perror("fopen"); exit(EXIT_FAILURE); }
    char * line = NULL;
    size_t alloc = 0;
    ssize_t n;
    while((n = getline(&line, &alloc, f)) != -1) {
      if(line[n - 1] == '\n') line[n - 1] = '\0';
      if(starts_with(line, "title ")) XStoreName(t->display, t->window, line+6);
    }
    fclose(f);
    free(line);
  }
  return NULL;
}

int main(int argc, char** argv) {
  // create window
  int W = 800;
  int H = 450;
  XSetErrorHandler(x_error_handler);
  Display * display = XOpenDisplay(NULL); if(display == NULL) { printf("XOpenDisplay\n"); exit(EXIT_FAILURE); }
  Drawable window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, W, H, 0, 0, 0);
  XSelectInput(display, window, FocusChangeMask);
  XMapWindow(display, window);
  cairo_surface_t* surface = cairo_xlib_surface_create(display, window, DefaultVisual(display, DefaultScreen(display)), W, H);
  cairo_t* g = cairo_create(surface);

  // create fifo and in another thread read its messages
  const char * server_fifo_path = "gfx-x11-cairo.fifo";
  if(unlink(server_fifo_path) == -1 && errno != ENOENT) { perror("unlink"); exit(EXIT_FAILURE); }
  if(mkfifo(server_fifo_path, S_IRUSR | S_IWUSR) == -1) { perror("mkfifo"); exit(EXIT_FAILURE); }
  struct shared_amongst_thread_t t;
  t.server_fifo_path = server_fifo_path;
  t.display = display;
  t.window = window;
  t.running = true;
  pthread_t fifo_thread;
  pthread_create(&fifo_thread, NULL, handle_fifo_loop, &t);

  // main loop
  int frame_count = 0;
  uint64_t t0 = currentTimeMillis();
  while(t.running) {
    // events
    while(XPending(display)) {
      XEvent event;
      XNextEvent(display, &event);
      switch(event.type) {
        case FocusIn:
          printf("FocusIn\n");
          break;
        case FocusOut:
          printf("FocusOut\n");
          break;
      }
    }

    // time flow
    uint64_t t1 = currentTimeMillis();
    double delta_time = (t1 - t0) / 1000.0;
    if (delta_time < (1.0 / 60)) continue; // commented out, don't cap
    t0 = t1;
    // fps
    frame_count++;
    //printf("%d: %f\n", frame_count, 1 / delta_time);

    // cairo clear
    cairo_set_source_rgb(g, 1, 1, 1);
    cairo_paint(g);
    
    // flush
    cairo_surface_flush(surface);
    XFlush(display);
  }

  // cleanup
  cairo_surface_destroy(surface);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  if(unlink(server_fifo_path) == -1) { perror("unlink"); exit(EXIT_FAILURE); }
  return 0;
}
