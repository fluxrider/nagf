// gcc -o evt-x11 evt-x11.c -Wall $(pkg-config --libs --cflags x11) -lpthread
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

#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

int x_error_handler(Display * d, XErrorEvent * e) {
  printf("evt-x11 x_error_handler: %d\n", e->error_code);
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
  XSelectInput(display, window, ButtonPressMask | KeyPressMask | KeyReleaseMask);
  XMapWindow(display, window);

  // create and in another thread read its messages
  const char * server_fifo_path = "evt-x11.fifo";
  if(unlink(server_fifo_path) == -1 && errno != ENOENT) { perror("unlink"); exit(EXIT_FAILURE); }
  if(mkfifo(server_fifo_path, S_IRUSR | S_IWUSR) == -1) { perror("mkfifo"); exit(EXIT_FAILURE); }
  struct shared_amongst_thread_t t;
  t.server_fifo_path = server_fifo_path;
  t.display = display;
  t.window = window;
  t.running = true;
  pthread_t fifo_thread;
  pthread_create(&fifo_thread, NULL, handle_fifo_loop, &t);

  // listen to x11 events
  while(t.running) {
    XEvent event;
    XNextEvent(display, &event);
    switch(event.type) {
      // keyboard
      case KeyRelease: {
        KeySym key = XkbKeycodeToKeysym(display, event.xkey.keycode, 0, event.xkey.state & ShiftMask ? 1 : 0);
        switch(key) {
          case XK_Escape: t.running = false; break;
          case XK_T: XStoreName(display, window, "cool Title"); printf("T\n"); break;
          case XK_t: XStoreName(display, window, "cool title"); printf("t\n"); break;
        }
        break;
      }
      // mouse
      case ButtonPress:
        printf("You pressed a button at (%d,%d)\n", event.xbutton.x, event.xbutton.y);
        break;
    }
  }

  // cleanup
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  if(unlink(server_fifo_path) == -1) { perror("unlink"); exit(EXIT_FAILURE); }
  return 0;
}
