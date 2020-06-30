// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later.
// gcc -o gfx-glfw gfx-glfw.c gfx-glfw_freetype-gl/*.c $(pkg-config --libs --cflags x11 opengl glfw3 glew freetype2 MagickWand) -lpthread -lm && ./gfx-glfw
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
#include <MagickWand/MagickWand.h>
#include <GL/glew.h>
#include "gfx-glfw_freetype-gl/freetype-gl.h"
#include "gfx-glfw_freetype-gl/mat4.h"
#include "gfx-glfw_freetype-gl/shader.h"
#include "gfx-glfw_freetype-gl/vertex-buffer.h"
#include <GLFW/glfw3.h>

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

// --------------------------------------------------------------- add_text ---
static void add_text(vertex_buffer_t * buffer, texture_font_t * font, const char * text, vec4 * color, vec2 * pen) {
  size_t i;
  float r = color->red, g = color->green, b = color->blue, a = color->alpha;
  size_t len = strlen(text);
  for(i = 0; i < len; ++i) {
    texture_glyph_t *glyph = texture_font_get_glyph(font, text + i);
    if( glyph != NULL ) {
      float kerning = i > 0? texture_glyph_get_kerning( glyph, text + i - 1 ) : 0.0f;
      pen->x += kerning;
      int x0  = (int)( pen->x + glyph->offset_x );
      int y0  = (int)( pen->y + glyph->offset_y );
      int x1  = (int)( pen->x + glyph->offset_x + glyph->width );
      int y1  = (int)( pen->y + glyph->offset_y - glyph->height );
      float s0 = glyph->s0;
      float t0 = glyph->t0;
      float s1 = glyph->s1;
      float t1 = glyph->t1;
      GLuint indices[6] = {0,1,2, 0,2,3};
      struct { float x, y, z; float s, t; float r, g, b, a; } vertices[4] = {
        { x0,y0,0, s0,t0, r,g,b,a },
        { x0,y1,0, s0,t1, r,g,b,a },
        { x1,y1,0, s1,t1, r,g,b,a },
        { x1,y0,0, s1,t0, r,g,b,a }
      };
      vertex_buffer_push_back( buffer, vertices, 4, indices, 6 );
      pen->x += glyph->advance_x;
    }
  }
}

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

  mat4 model, view, projection;
  mat4_set_identity(&projection);
  mat4_set_identity(&model);
  mat4_set_identity(&view);

  // font
  texture_atlas_t * font_atlas = texture_atlas_new(1024, 1024, 1);
  texture_font_t * font = texture_font_new_from_file(font_atlas, 70, "/usr/share/fonts/TTF/DejaVuSans.ttf");
  glGenTextures(1, &font_atlas->id);
  glBindTexture(GL_TEXTURE_2D, font_atlas->id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  vertex_buffer_t * text_buffer = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
  GLuint font_shader = shader_load("gfx-glfw_freetype-gl/v3f-t2f-c4f.vert", "gfx-glfw_freetype-gl/v3f-t2f-c4f.frag"); // TODO relative path to CWD... nah, just embed them

  // images
  GLuint img_shader = shader_load("gfx-glfw_freetype-gl/tmp.vert", "gfx-glfw_freetype-gl/tmp.frag"); // TODO embed them
  GLuint my_img;
  glGenTextures(1, &my_img);
  glBindTexture(GL_TEXTURE_2D, my_img);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  vertex_buffer_t * img_buffer = vertex_buffer_new("my_position:3f,my_tex_uv:2f");

  //GLubyte pixels[4] = {0,255,0,100};
  //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  MagickWandGenesis();
  MagickWand * magick = NewMagickWand();
  if(MagickReadImage(magick, "../demos/zeldaish/princess.png") == MagickFalse) { ExceptionType et; char * e = MagickGetException(magick,&et); fprintf(stderr,"MagickReadImage %s\n",e); MagickRelinquishMemory(e); exit(EXIT_FAILURE); }
  MagickBooleanType retval = MagickSetImageFormat(magick, "RGBA");
  printf("M %d\n", retval == MagickTrue);
  size_t img_blob_length;
  unsigned char * img_blob = MagickGetImagesBlob(magick, &img_blob_length);
  printf("M %p %d\n", img_blob, (int)img_blob_length);
  Image * image = GetImageFromMagickWand(magick);
  printf("image %p\n", image);
  printf("image w:%d h:%d\n", image->columns, image->rows);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->columns, image->rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_blob);
  //MagickDestroyImage(image); // do I need to do this? when is it safe to do so?
  DestroyMagickWand(magick);
  MagickWandTerminus();

  // main loop
  double t0 = glfwGetTime();
  while(!glfwWindowShouldClose(window)) {
    double t1 = glfwGetTime();
    double delta = t1 - t0;
    t0 = t1;
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    mat4_set_orthographic(&projection, 0, width, 0, height, -1, 1);

    glClearColor(.5, .5, .5, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(font_shader);
    glUniform1i(glGetUniformLocation(font_shader, "texture"), 0);
    glUniformMatrix4fv(glGetUniformLocation(font_shader, "model"), 1, 0, model.data);
    glUniformMatrix4fv(glGetUniformLocation(font_shader, "view"), 1, 0, view.data);
    glUniformMatrix4fv(glGetUniformLocation(font_shader, "projection"), 1, 0, projection.data);
    glBindTexture(GL_TEXTURE_2D, font_atlas->id);
    
    vec4 black = {{0,0,0,1}};
    vec4 white = {{1,1,1,1}};
    char text[256];
    snprintf(text, 256, "fps %f", 1 / delta);
    {
      vec2 pen = {{200,100}};
      font->rendermode = RENDER_OUTLINE_NEGATIVE;
      font->outline_thickness = 1;
      add_text(text_buffer, font, text, &white, &pen);
    }
    {
      vec2 pen = {{200,100}};
      font->rendermode = RENDER_OUTLINE_EDGE;
      font->outline_thickness = 1;
      add_text(text_buffer, font, text, &black, &pen);
    }
    // TODO only upload atlas if it changed
    glBindTexture(GL_TEXTURE_2D, font_atlas->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font_atlas->width, font_atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, font_atlas->data);
    vertex_buffer_render(text_buffer, GL_TRIANGLES);
    vertex_buffer_clear(text_buffer);
    
    // image
    glUseProgram(img_shader);
    glUniform1i(glGetUniformLocation(img_shader, "my_sampler"), 0);
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glUniformMatrix4fv(glGetUniformLocation(img_shader, "my_model"), 1, 0, model.data);
    glUniformMatrix4fv(glGetUniformLocation(img_shader, "my_projection"), 1, 0, projection.data);
    glBindTexture(GL_TEXTURE_2D, my_img);
    int x0  = 10;
    int y0  = 300;
    int x1  = 150;
    int y1  = 10;
    GLuint indices[6] = {0,1,2, 0,2,3};
    struct { float x, y, z; float s, t; } vertices[4] = {
      { x0,y0,0, 0,0 },
      { x0,y1,0, 0,1 },
      { x1,y1,0, 1,1 },
      { x1,y0,0, 1,0 }
    };
    vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
    vertex_buffer_render(img_buffer, GL_TRIANGLES);
    vertex_buffer_clear(img_buffer);
    
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // cleanup
  texture_font_delete(font);
  vertex_buffer_delete(text_buffer);
  vertex_buffer_delete(img_buffer);
  glDeleteTextures(1, &font_atlas->id);
  texture_atlas_delete(font_atlas);
  
  //if(unlink(server_fifo_path) == -1) { perror("unlink"); exit(EXIT_FAILURE); }
  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
