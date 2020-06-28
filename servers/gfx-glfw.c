// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later.
// gcc -o gfx-glfw gfx-glfw.c $(pkg-config --libs --cflags x11 opengl glfw3 glew freetype2 MagickWand) -lpthread && ./gfx-glfw
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <MagickWand/MagickWand.h>

bool starts_with(const char * s, const char * start) {
  return strncmp(start, s, strlen(start)) == 0;
}

struct shared_amongst_thread_t {
  const char * server_fifo_path;
};

/*
void * handle_fifo_loop(void * vargp) {
  struct shared_amongst_thread_t * t = vargp;
  while(t->running) {
    FILE * f = fopen(t->server_fifo_path, "r"); if(!f) { perror("fopen"); exit(EXIT_FAILURE); }
    char * line = NULL;
    size_t alloc = 0;
    ssize_t n;
    while((n = getline(&line, &alloc, f)) != -1) {
      if(line[n - 1] == '\n') line[n - 1] = '\0';
      //if(starts_with(line, "title ")) TODO;
    }
    fclose(f);
    free(line);
  }
  return NULL;
}
*/

static void glfw_error_callback( int error, const char* description ) {
  fputs( description, stderr );
}

int main(int argc, char** argv) {
  // create fifo and in another thread read its messages
  /*
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
  */
  MagickWandGenesis();
  MagickWand * magick = NewMagickWand();
  if(MagickReadImage(magick, "../demos/zeldaish/princess.png") == MagickFalse) { ExceptionType et; char * e = MagickGetException(magick,&et); fprintf(stderr,"MagickReadImage %s\n",e); MagickRelinquishMemory(e); exit(EXIT_FAILURE); }
  DestroyMagickWand(magick);
  MagickWandTerminus();

  glfwSetErrorCallback(glfw_error_callback);
  if(!glfwInit()) { fprintf(stderr, "glfwInit\n"); exit(EXIT_FAILURE); }
  glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
  glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
  GLFWwindow * window = glfwCreateWindow(800, 450, argv[0], NULL, NULL); if(!window) { fprintf(stderr, "glfwCreateWindow\n"); exit(EXIT_FAILURE); }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(0); // no-vsync
  glewExperimental = GL_TRUE;
  GLenum err = glewInit(); if(GLEW_OK != err) { fprintf(stderr, "glewInit() %s\n", glewGetErrorString(err)); exit(EXIT_FAILURE); }
  glfwShowWindow(window);

  // main loop
  double t0 = glfwGetTime();
  while(!glfwWindowShouldClose(window)) {
    double t1 = glfwGetTime();
    double delta = t1 - t0;
    t0 = t1;
    glClearColor(.5, .5, .5, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // cleanup
  //if(unlink(server_fifo_path) == -1) { perror("unlink"); exit(EXIT_FAILURE); }
  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
