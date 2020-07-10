// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later.
// xxd -i < gfx-glfw_freetype-gl/v3f-t2f-c4f.vert > text.vert.xxd && xxd -i < gfx-glfw_freetype-gl/v3f-t2f-c4f.frag > text.frag.xxd && xxd -i < gfx-glfw.img.vert > img.vert.xxd && xxd -i < gfx-glfw.img.frag > img.frag.xxd && gcc -o gfx-glfw gfx-glfw.c gfx-glfw_freetype-gl/*.c $(pkg-config --libs --cflags x11 opengl glfw3 glew freetype2 MagickWand) -lpthread -lm && rm *.xxd && ./gfx-glfw
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <MagickWand/MagickWand.h>
#include <GL/glew.h>
#include "gfx-glfw_freetype-gl/freetype-gl.h"
#include "gfx-glfw_freetype-gl/mat4.h"
#include "gfx-glfw_freetype-gl/shader.h"
#include "gfx-glfw_freetype-gl/vertex-buffer.h"
#include <GLFW/glfw3.h>
#include "srr.h"
#include "gfx-util.h"
#include "data-util.h"

static void glfw_error_callback(int error, const char * description) {
  fprintf(stderr, "GFX error: glfw_error_callback %s\n", description); exit(EXIT_FAILURE);
}

struct res {
  uint8_t type;
  union {
    struct {
      texture_atlas_t * atlas;
      struct dict * fonts;
    };
    struct {
      double progress;
      uint8_t progress_type;
    };
    struct {
      GLuint texture;
      int w;
      int h;
    };
    struct {
      char error[3];
    };
  };
};

static void add_text(vertex_buffer_t * buffer, double fw, double fh, texture_font_t * font, const char * text, vec4 * color, double x, double y) {
  size_t i;
  float r = color->red, g = color->green, b = color->blue, a = color->alpha;
  size_t len = strlen(text);
  for(i = 0; i < len; ++i) {
    texture_glyph_t * glyph = texture_font_get_glyph(font, text + i);
    if(glyph) {
      float kerning = i > 0? texture_glyph_get_kerning( glyph, text + i - 1 ) : 0.0f;
      x += kerning * fw;
      double x0  = (x + glyph->offset_x * fw);
      double y0  = (y - glyph->offset_y * fh);
      double x1  = (x + glyph->offset_x * fw + glyph->width * fw);
      double y1  = (y - glyph->offset_y * fh + glyph->height * fh);
      // half-pixel correction-ish to reduce chance of filtering artefact
      double fudge_x = 0; // .3 / font->atlas->width;
      double fudge_y = 0; // .3 / font->atlas->height;
      float s0 = glyph->s0 + fudge_x;
      float t0 = glyph->t0 + fudge_y;
      float s1 = glyph->s1 - fudge_x;
      float t1 = glyph->t1 - fudge_y;
      GLuint indices[6] = {0,1,2, 0,2,3};
      struct { float x, y; float s, t; float r, g, b, a; } vertices[4] = {
        { x0,y0, s0,t0, r,g,b,a },
        { x0,y1, s0,t1, r,g,b,a },
        { x1,y1, s1,t1, r,g,b,a },
        { x1,y0, s1,t0, r,g,b,a }
      };
      vertex_buffer_push_back( buffer, vertices, 4, indices, 6 );
      x += glyph->advance_x * fw;
    }
  }
}

static GLuint shader_load_from_src(const char * vert_source, const char * frag_source) {
  GLuint handle = glCreateProgram();

  GLuint vert_shader = shader_compile(vert_source, GL_VERTEX_SHADER);
  glAttachShader(handle, vert_shader);
  glDeleteShader(vert_shader);

  GLuint frag_shader = shader_compile(frag_source, GL_FRAGMENT_SHADER);
  glAttachShader(handle, frag_shader);
  glDeleteShader(frag_shader);

  glLinkProgram(handle);

  GLint link_status;
  glGetProgramiv(handle, GL_LINK_STATUS, &link_status);
  if(!link_status) {
    GLchar messages[256];
    glGetProgramInfoLog(handle, sizeof(messages), 0, messages);
    fprintf(stderr, "GFX error: %s\n", messages);
    exit(EXIT_FAILURE);
  }
  return handle;
}

static bool starts_with(const char * s, const char * start) {
  return strncmp(start, s, strlen(start)) == 0;
}

static bool ends_with(const char * s, const char * end) {
  size_t s_len = strlen(s);
  size_t end_len = strlen(end);
  if(end_len > s_len) return false;
  return strncmp(s + s_len - end_len, end, end_len) == 0;
}

static bool str_equals(const char * s, const char * s2) {
  return strcmp(s, s2) == 0;
}

struct shared_amongst_thread_t {
  GLFWwindow * window;
  const char * srr_path;
  bool running;
  bool focused;
  int W;
  int H;
  int aspectW;
  int aspectH;
  int preferred_W;
  int preferred_H;
  struct dict cache;
  pthread_mutex_t cache_mutex;
  sem_t flush_pre;
  sem_t flush_post;
  sem_t should_quit;
  bool first_flush;
};

static char * str_trim(char * s) {
  while(isspace(*s)) s++;
  if(*s == '\0') return s; // all space
  char * end = s + strlen(s) - 1;
  while(end > s && isspace(*end)) end--;
  end[1] = '\0';
  return s;
}

static void parse_color(const char * color, double * r, double * g, double * b, double * a) {
  char color_2[3]; color_2[2] = '\0';
  *a = 1;
  int i = 0;
  if(strlen(color) == 8) {
    color_2[0] = color[i++]; color_2[1] = color[i++];
    *a = strtol(color_2, NULL, 16) / 255.0;
  }
  color_2[0] = color[i++]; color_2[1] = color[i++];
  *r = strtol(color_2, NULL, 16) / 255.0;
  color_2[0] = color[i++]; color_2[1] = color[i++];
  *g = strtol(color_2, NULL, 16) / 255.0;
  color_2[0] = color[i++]; color_2[1] = color[i++];
  *b = strtol(color_2, NULL, 16) / 255.0;
}

static void * handle_fifo_loop(void * vargp) {
  // this is the 'main' thread as far as glfw goes
  printf("GFX fifo thread\n");
  struct shared_amongst_thread_t * t = vargp;
  char * _line = NULL;
  size_t alloc = 0;
  char * title = NULL;
  GLuint fill_shader;
  GLuint img_shader;
  GLuint font_shader;
  vertex_buffer_t * fill_buffer = NULL;
  vertex_buffer_t * img_buffer = NULL;
  vertex_buffer_t * font_buffer = NULL;
  int old_w = 0, old_h = 0;

  // matrices
  mat4 model, projection;
  mat4_set_identity(&projection);
  mat4_set_identity(&model);

  while(t->running) {
    FILE * f = fopen("gfx.fifo", "r"); if(!f) { perror("GFX error: fopen"); exit(EXIT_FAILURE); }
    ssize_t n;
    while((n = getline(&_line, &alloc, f)) != -1) {
      if(_line[n - 1] == '\n') _line[n - 1] = '\0';
      char * line = str_trim(_line);
      if(str_equals(line, "flush")) {
        //printf("GFX fifo flush\n");
        // finish setting up window on first flush
        if(t->first_flush) {
          t->first_flush = false;
          // show window
          bool no_pref = t->preferred_W == 0;
          // TODO stretch window if(no_pref) frame.setExtendedState(JFrame.MAXIMIZED_BOTH); 
          glfwShowWindow(t->window); // TODO This function must only be called from the main thread (for portability).
        }
        // on flush, stop handling any more messages until srr thread completes the flush
        if(sem_post(&t->flush_pre) == -1) { perror("GFX error: sem_post"); exit(EXIT_FAILURE); }
        if(sem_wait(&t->flush_post) == -1) { perror("GFX error: sem_wait"); exit(EXIT_FAILURE); }
        // flush our drawing to the screen
        if(t->running) {
          glfwSwapBuffers(t->window);
          glfwPollEvents(); // TODO This function must only be called from the main thread (for portability).
          // on resize
          int width, height;
          glfwGetFramebufferSize(t->window, &width, &height);
          if(old_w != width || old_h != height) {
            old_w = width;
            old_h = height;
            // respect aspect ratio (i.e. black bars)
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            double a = t->W / (double) t->H;
            double A = width / (double) height;
            int offsetX = 0;
            int offsetY = 0;
            t->aspectW = width;
            t->aspectH = height;
            // top/down black bars
            if(a / A > 1) {
              t->aspectH = width * t->H / t->W;
              offsetY = (height - t->aspectH) / 2;
            }
            // left/right black bars
            else {
              t->aspectW = height * t->W / t->H;
              offsetX = (width - t->aspectW) / 2;
            }
            glViewport(offsetX, offsetY, t->aspectW, t->aspectH);
            // clear all font atlas
            if(pthread_mutex_lock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
            for(size_t res_index = 0; res_index < t->cache.size; res_index++) {
              struct res * res = dict_get_by_index(&t->cache, res_index);
              if(res->type == GFX_STAT_FNT) {
                texture_atlas_clear(res->atlas);
                for(size_t font_index = 0; font_index < res->fonts->size; font_index++) {
                  texture_font_t * font = dict_get_by_index(res->fonts, font_index);
                  texture_font_delete(font);
                }
                dict_free(res->fonts);
                dict_init(res->fonts, 0, false, false);
              }
            }
            if(pthread_mutex_unlock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
          }
        }
      } else if(str_equals(line, "hq")) {
        printf("GFX fifo hq\n");
      } else if(starts_with(line, "title ")) {
        printf("GFX fifo title\n");
        if(t->window) glfwSetWindowTitle(t->window, &line[6]); // TODO This function must only be called from the main thread (for portability).
        else title = strdup(&line[6]);
      } else if(starts_with(line, "window ")) {
        printf("GFX fifo window\n");
        char * line_sep = &line[7];
        t->W = t->aspectW = strtol(strsep(&line_sep, " "), NULL, 10);
        t->H = t->aspectH = strtol(strsep(&line_sep, " "), NULL, 10);
        const char * token = strsep(&line_sep, " ");
        if(token) {
          t->preferred_W = strtol(token, NULL, 10);
          t->preferred_H = strtol(strsep(&line_sep, " "), NULL, 10);
        } else {
          t->preferred_W = 0;
          t->preferred_H = 0;
        }
        mat4_set_orthographic(&projection, 0, t->W, t->H, 0, -1, 1);
        // create window
        glfwSetErrorCallback(glfw_error_callback);
        printf("GFX glfwInit\n");
        if(!glfwInit()) { fprintf(stderr, "GFX error: glfwInit\n"); exit(EXIT_FAILURE); } // // TODO This function must only be called from the main thread (for portability).
        printf("GFX create window\n");
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE); // TODO This function must only be called from the main thread (for portability).
        glfwWindowHint(GLFW_RESIZABLE, GL_TRUE); // TODO This function must only be called from the main thread (for portability).
        t->window = glfwCreateWindow(t->W, t->H, title? title : NULL, NULL, NULL); if(!t->window) { fprintf(stderr, "GFX error: glfwCreateWindow\n"); exit(EXIT_FAILURE); } // TODO This function must only be called from the main thread (for portability).
        glfwMakeContextCurrent(t->window);
        glfwSwapInterval(0); // no-vsync
        printf("GFX glew\n");
        glewExperimental = GL_TRUE;
        GLenum err = glewInit(); if(GLEW_OK != err) { fprintf(stderr, "GFX error: glewInit() %s\n", glewGetErrorString(err)); exit(EXIT_FAILURE); }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // fill shader
        printf("GFX load fill shader\n");
        char fill_shader_vert[] = {
        #include "fill.vert.xxd"
        , 0 };
        char fill_shader_frag[] = {
        #include "fill.frag.xxd"
        , 0 };
        fill_shader = shader_load_from_src(fill_shader_vert, fill_shader_frag);
        fill_buffer = vertex_buffer_new("my_position:2f");
        // img shader
        printf("GFX load image shader\n");
        char img_shader_vert[] = {
        #include "img.vert.xxd"
        , 0 };
        char img_shader_frag[] = {
        #include "img.frag.xxd"
        , 0 };
        img_shader = shader_load_from_src(img_shader_vert, img_shader_frag);
        img_buffer = vertex_buffer_new("my_position:2f,my_tex_uv:2f");
        // font shader
        printf("GFX load font shader\n");
        char text_shader_vert[] = {
        #include "font.vert.xxd"
        , 0 };
        char text_shader_frag[] = {
        #include "font.frag.xxd"
        , 0 };
        font_shader = shader_load_from_src(text_shader_vert, text_shader_frag);
        font_buffer = vertex_buffer_new("vertex:2f,tex_coord:2f,color:4f");
      } else if(starts_with(line, "cache ")) {
        const char * path = &line[6];
        if(pthread_mutex_lock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
        if(dict_has(&t->cache, path)) printf("GFX cache refresh %s\n", path); else {
          if(ends_with(path, ".ttf")) {
            // cache font TODO async
            printf("GFX load font %s\n", path);
            struct res res;
            res.type = GFX_STAT_FNT;
            res.atlas = texture_atlas_new(1024, 1024, 1);
            res.fonts = malloc(sizeof(struct dict));
            dict_init(res.fonts, 0, false, false);
            glGenTextures(1, &res.atlas->id);
            glBindTexture(GL_TEXTURE_2D, res.atlas->id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            dict_set(&t->cache, path, &res);
          } else {
            // cache image TODO async
            printf("GFX load image %s\n", path);
            MagickWandGenesis();
            MagickWand * magick = NewMagickWand();
            if(MagickReadImage(magick, path) == MagickFalse) { ExceptionType et; char * e = MagickGetException(magick,&et); fprintf(stderr,"MagickReadImage %s\n",e); MagickRelinquishMemory(e); exit(EXIT_FAILURE); }
            MagickBooleanType retval = MagickSetImageFormat(magick, "RGBA");
            size_t img_blob_length;
            unsigned char * img_blob = MagickGetImagesBlob(magick, &img_blob_length);
            Image * image = GetImageFromMagickWand(magick);
            GLuint my_img;
            glGenTextures(1, &my_img);
            glBindTexture(GL_TEXTURE_2D, my_img);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->columns, image->rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_blob);
            RelinquishMagickMemory(img_blob);
            DestroyMagickWand(magick);
            MagickWandTerminus();
            struct res res;
            res.type = GFX_STAT_IMG;
            res.texture = my_img;
            res.w = image->columns;
            res.h = image->rows;
            dict_set(&t->cache, path, &res);
          }
        }
        if(pthread_mutex_unlock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
      } else if(starts_with(line, "draw ")) {
        char * line_sep = &line[5];
        // normal: draw path x y
        const char * path = strsep(&line_sep, " ");
        if(pthread_mutex_lock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
        if(dict_has(&t->cache, path)) {
          struct res * res = dict_get(&t->cache, path);
          glUseProgram(img_shader);
          glUniform1i(glGetUniformLocation(img_shader, "my_sampler"), 0);
          glEnable(GL_TEXTURE_2D);
          glActiveTexture(GL_TEXTURE0);
          glUniformMatrix4fv(glGetUniformLocation(img_shader, "my_model"), 1, 0, model.data);
          glUniformMatrix4fv(glGetUniformLocation(img_shader, "my_projection"), 1, 0, projection.data);
          glBindTexture(GL_TEXTURE_2D, res->texture);
          // half-pixel correction-ish to reduce chance of filtering artefact
          double fudge = 0;
          GLuint indices[6] = {0,1,2, 0,2,3};
          double p1 = strtod(strsep(&line_sep, " "), NULL);
          double p2 = strtod(strsep(&line_sep, " "), NULL);
          if(!line_sep) {
            struct { float x, y; float s, t; } vertices[4] = {
              { p1, p2,                    0,0 },
              { p1, p2 + res->h,           0,1 },
              { p1 + res->w, p2 + res->h,  1,1 },
              { p1 + res->w, p2,           1,0 }
            };
            vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
          } else {
            // scaled: draw path x y w h
            double p3 = strtod(strsep(&line_sep, " "), NULL);
            double p4 = strtod(strsep(&line_sep, " "), NULL);
            if(!line_sep) {
              struct { float x, y; float s, t; } vertices[4] = {
                { p1, p2,            0,0 },
                { p1, p2 + p4,       0,1 },
                { p1 + p3, p2 + p4,  1,1 },
                { p1 + p3, p2,       1,0 }
              };
              vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
            } else {
              // region: draw path sx sy w h x y (mx=mirror-x)
              double p5 = strtod(strsep(&line_sep, " "), NULL);
              double p6 = strtod(strsep(&line_sep, " "), NULL);
              if(!line_sep) {
                struct { float x, y; float s, t; } vertices[4] = {
                  { p5, p6,            (p1 + fudge) / res->w, (p2 + fudge) / res->h },
                  { p5, p6 + p4,       (p1 + fudge) / res->w, (p2 + p4 - fudge) / res->h },
                  { p5 + p3, p6 + p4,  (p1 + p3 - fudge) / res->w, (p2 + p4 - fudge) / res->h },
                  { p5 + p3, p6,       (p1 + p3 - fudge) / res->w, (p2 + fudge) / res->h }
                };
                vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
              } else {
                const char * tmp = strsep(&line_sep, " ");
                if(strcmp(tmp, "mx") == 0) {
                  struct { float x, y; float s, t; } vertices[4] = {
                    { p5, p6,            (p1 + p3 - fudge) / res->w, (p2 + fudge) / res->h },
                    { p5, p6 + p4,       (p1 + p3 - fudge) / res->w, (p2 + p4 - fudge) / res->h },
                    { p5 + p3, p6 + p4,  (p1 + fudge) / res->w, (p2 + p4 - fudge) / res->h },
                    { p5 + p3, p6,       (p1 + fudge) / res->w, (p2 + fudge) / res->h }
                  };
                  vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
                } else {
                  // region: draw path sx sy sw sh x y w h
                  double p7 = strtod(tmp, NULL);
                  double p8 = strtod(strsep(&line_sep, " "), NULL);
                  struct { float x, y; float s, t; } vertices[4] = {
                    { p5, p6,            (p1 + fudge) / res->w, (p2 + fudge) / res->h },
                    { p5, p6 + p8,       (p1 + fudge) / res->w, (p2 + p4 - fudge) / res->h },
                    { p5 + p7, p6 + p8,  (p1 + p3 - fudge) / res->w, (p2 + p4 - fudge) / res->h },
                    { p5 + p7, p6,       (p1 + p3 - fudge) / res->w, (p2 + fudge) / res->h }
                  };
                  vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
                }
              }
            }
          }
          vertex_buffer_render(img_buffer, GL_TRIANGLES); // TODO delay render
          vertex_buffer_clear(img_buffer);
        }
        if(pthread_mutex_unlock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
      } else if(starts_with(line, "text ")) {
        char * line_sep = &line[5];
        const char * path = strsep(&line_sep, " ");
        if(pthread_mutex_lock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
        if(dict_has(&t->cache, path)) {
          // text font x y w h valign halign line_count clip scroll outline_color fill_color outline_size message
          struct res * res = dict_get(&t->cache, path);
          glUseProgram(font_shader);
          glUniform1i(glGetUniformLocation(font_shader, "texture"), 0);
          glUniformMatrix4fv(glGetUniformLocation(font_shader, "model"), 1, 0, model.data);
          glUniformMatrix4fv(glGetUniformLocation(font_shader, "projection"), 1, 0, projection.data);
          glBindTexture(GL_TEXTURE_2D, res->atlas->id);
          
          bool tight = starts_with(line_sep, "tight ");
          if(tight) strsep(&line_sep, " ");
          double x = strtod(strsep(&line_sep, " "), NULL);
          double y = strtod(strsep(&line_sep, " "), NULL);
          double w = strtod(strsep(&line_sep, " "), NULL);
          double h = strtod(strsep(&line_sep, " "), NULL);
          const char * valign = strsep(&line_sep, " ");
          const char * halign = strsep(&line_sep, " ");
          int line_count = strtol(strsep(&line_sep, " "), NULL, 10);
          bool clip = strcmp(strsep(&line_sep, " "), "clip") == 0;
          double scroll = strtod(strsep(&line_sep, " "), NULL);
          double outline_r, outline_g, outline_b, outline_a;
          double fill_r, fill_g, fill_b, fill_a;
          parse_color(strsep(&line_sep, " "), &outline_r, &outline_g, &outline_b, &outline_a);
          parse_color(strsep(&line_sep, " "), &fill_r, &fill_g, &fill_b, &fill_a);
          double outline_size = strtod(strsep(&line_sep, " "), NULL);
          const char * message = line_sep;
          double line_height = h / line_count;
          vec4 outline = {{outline_r,outline_g,outline_b,outline_a}};
          vec4 fill = {{fill_r,fill_g,fill_b,fill_a}};
          
          texture_font_t * font;
          double font_size = line_height * t->aspectH / t->H;
          int font_key = (int)(font_size * 1000);
          if(dict_has(res->fonts, font_key)) {
            font = dict_get(res->fonts, font_key);
          } else {
            font = texture_font_new_from_file(res->atlas, font_size, path);
            dict_set(res->fonts, font_key, font);
          }
          
          // TODO line break and font size
          font->outline_thickness = outline_size;
          {
            font->rendermode = RENDER_OUTLINE_NEGATIVE;
            add_text(font_buffer, t->W / (double)t->aspectW, t->H / (double)t->aspectH, font, message, &fill, x, y + line_height);
          }
          {
            font->rendermode = RENDER_OUTLINE_EDGE;
            add_text(font_buffer, t->W / (double)t->aspectW, t->H / (double)t->aspectH, font, message, &outline, x, y + line_height);
          }
          // TODO only upload atlas if it changed
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, res->atlas->width, res->atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, res->atlas->data);
          vertex_buffer_render(font_buffer, GL_TRIANGLES); // TODO delay
          vertex_buffer_clear(font_buffer);
        }
        if(pthread_mutex_unlock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
      } else if(starts_with(line, "fill ")) {
        char * line_sep = &line[5];
        const char * color = strsep(&line_sep, " ");
        double r, g, b, a;
        parse_color(color, &r, &g, &b, &a);
        double x = strtod(strsep(&line_sep, " "), NULL);
        double y = strtod(strsep(&line_sep, " "), NULL);
        double w = strtod(strsep(&line_sep, " "), NULL);
        double h = strtod(strsep(&line_sep, " "), NULL);
        glUseProgram(fill_shader);
        glUniform4f(glGetUniformLocation(fill_shader, "my_color"), r, g, b, a);
        glUniformMatrix4fv(glGetUniformLocation(fill_shader, "my_model"), 1, 0, model.data);
        glUniformMatrix4fv(glGetUniformLocation(fill_shader, "my_projection"), 1, 0, projection.data);
        GLuint indices[6] = {0,1,2, 0,2,3};
        struct { float x, y; } vertices[4] = {
          { x,y },
          { x,y+h },
          { x+w,y+h },
          { x+w,y }
        };
        vertex_buffer_push_back(fill_buffer, vertices, 4, indices, 6);
        vertex_buffer_render(fill_buffer, GL_TRIANGLES); // TODO delay render
        vertex_buffer_clear(fill_buffer);
      }
    }
    fclose(f);
  }
  free(_line);
  free(title);
  if(sem_post(&t->should_quit) == -1) { perror("GFX error: sem_post"); exit(EXIT_FAILURE); }
  return NULL;
}

static void * handle_srr_loop(void * vargp) {
  struct shared_amongst_thread_t * t = vargp;

  // connect
  printf("GFX starting srr %s\n", t->srr_path);
  const char * error;
  struct srr server;
  error = srr_init(&server, t->srr_path, 8192, true, false, 3); if(error) { printf("GFX error: srr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct srr_direct * mem = srr_direct(&server);

  // wait for a message from the client
  char _buffer[8193];
  double t0 = glfwGetTime();
  double delta_time = 0;
  while(1) {
    // receive
    error = srr_receive(&server); if(error) { printf("GFX error: srr_receive: %s\n", error); exit(EXIT_FAILURE); }
    //printf("GFX srr sync\n");
    // copy message, because the reply will overwrite it as we parse
    memcpy(_buffer, mem->msg, mem->length);
    _buffer[mem->length] = '\0';
    // build reply
    int i = 0;
    if(t->window) t->running &= !glfwWindowShouldClose(t->window);
    mem->msg[i++] = t->focused;
    mem->msg[i++] = !t->running;
    *((int *)(&mem->msg[i])) = t->W; i+=4;
    *((int *)(&mem->msg[i])) = t->H; i+=4;
    // let fifo drain until flush to ensure all drawing have been done
    if(sem_wait(&t->flush_pre) == -1) { perror("GFX error: sem_wait"); exit(EXIT_FAILURE); }
    // delta_time
    double t1 = glfwGetTime();
    delta_time = t1 - t0;
    t0 = t1;
    // let fifo resume
    if(sem_post(&t->flush_post) == -1) { perror("GFX error: sem_post"); exit(EXIT_FAILURE); }
    // parse commands
    char * buffer = _buffer;
    char * command;
    while(command = strsep(&buffer, " ")) {
      if(str_equals(command, "delta")) {
        mem->msg[i++] = GFX_STAT_DLT;
        *((int *)(&mem->msg[i])) = (int)(delta_time * 1000); i+=4;
      } else if(str_equals(command, "stat")) {
        const char * path = strsep(&buffer, " ");
        if(pthread_mutex_lock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
        if(dict_has(&t->cache, path)) {
          struct res * res = dict_get(&t->cache, path);
          if(res->type == GFX_STAT_ERR) {
            mem->msg[i++] = GFX_STAT_ERR; mem->msg[i++] = res->error[0]; mem->msg[i++] = res->error[1]; mem->msg[i++] = res->error[2];
          } else if(res->type == GFX_STAT_IMG) {
            mem->msg[i++] = GFX_STAT_IMG;
            *((int *)(&mem->msg[i])) = res->w; i+=4;
            *((int *)(&mem->msg[i])) = res->h; i+=4;
          } else if(res->type == GFX_STAT_FNT) {
            mem->msg[i++] = GFX_STAT_FNT;
            *((int *)(&mem->msg[i])) = 1; i+=4;
            *((int *)(&mem->msg[i])) = 1; i+=4;
          } else if(res->type == GFX_STAT_COUNT) {
            mem->msg[i++] = res->progress_type;
            *((int *)(&mem->msg[i])) = 0; i+=4;
            *((int *)(&mem->msg[i])) = (int)(res->progress * 1000); i+=4;
          }
        } else {
          mem->msg[i++] = GFX_STAT_ERR; mem->msg[i++] = 'E'; mem->msg[i++] = ' '; mem->msg[i++] = ' ';
        }
        if(pthread_mutex_unlock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
      } else if(str_equals(command, "statall")) {
        // check the state of everything in the cache to do a cummulative progress
        int p = 0;
        bool progress_error = false;
        if(pthread_mutex_lock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
        for(size_t index = 0; !progress_error && index < t->cache.size; index++) {
          struct res * res = dict_get_by_index(&t->cache, index);
          if(!res) continue;
          else if(res->type == GFX_STAT_ERR) {
            mem->msg[i++] = GFX_STAT_ERR; mem->msg[i++] = res->error[0]; mem->msg[i++] = res->error[1]; mem->msg[i++] = res->error[2];
            progress_error = true;
          } else if(res->type == GFX_STAT_COUNT) {
            p += (int)(res->progress * 1000);
          } else {
            p += 1000;
          }
        }
        if(!progress_error) {
          mem->msg[i++] = GFX_STAT_ALL;
          *((int *)(&mem->msg[i])) = ((t->cache.size == 0)? 0 : p / t->cache.size); i+=4;
        }
        if(pthread_mutex_unlock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
      }
    }
    // reply
    error = srr_reply(&server, i); if(error) { fprintf(stderr, "GFX error: srr_reply: %s\n", error); exit(EXIT_FAILURE); }
    if(!t->running) break;
  }

  error = srr_disconnect(&server); if(error) { fprintf(stderr, "GFX error: srr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
  if(sem_post(&t->should_quit) == -1) { perror("GFX error: sem_post"); exit(EXIT_FAILURE); }
  return NULL;
}

int main(int argc, char** argv) {
  struct shared_amongst_thread_t * t = malloc(sizeof(struct shared_amongst_thread_t)); // place it on heap, as it is unclear is threads can properly access stack var
  t->window = NULL;
  t->running = true;
  t->first_flush = true;
  t->focused = true;
  dict_init(&t->cache, sizeof(struct res), true, true);
  if(pthread_mutex_init(&t->cache_mutex, NULL) != 0) { fprintf(stderr, "GFX error: pthread_mutex_init\n"); exit(EXIT_FAILURE); }
  if(sem_init(&t->flush_pre, 0, 0) == -1) { perror("GFX error: sem_init"); exit(EXIT_FAILURE); }
  if(sem_init(&t->flush_post, 0, 0) == -1) { perror("GFX error: sem_init"); exit(EXIT_FAILURE); }
  if(sem_init(&t->should_quit, 0, 0) == -1) { perror("GFX error: sem_init"); exit(EXIT_FAILURE); }

  // create fifo and in another thread read its messages
  printf("GFX fifo\n");
  if(unlink("gfx.fifo") == -1 && errno != ENOENT) { perror("GFX error: unlink"); exit(EXIT_FAILURE); }
  if(mkfifo("gfx.fifo", S_IRUSR | S_IWUSR) == -1) { perror("GFX error: mkfifo"); exit(EXIT_FAILURE); }
  pthread_t fifo_thread;
  pthread_create(&fifo_thread, NULL, handle_fifo_loop, t);

  // create srr server in another thread and listen to messages
  printf("GFX srr\n");
  if(argc != 2) { fprintf(stderr, "GFX error: must specify the srr shm name as arg\n"); exit(EXIT_FAILURE); }
  t->srr_path = argv[1];
  pthread_t srr_thread;
  pthread_create(&srr_thread, NULL, handle_srr_loop, t);

  // wait for signal to quit
  if(sem_wait(&t->should_quit) == -1) { perror("GFX error: sem_wait"); exit(EXIT_FAILURE); }

  // cleanup
  //texture_font_delete(font);
  //vertex_buffer_delete(text_buffer);
  //vertex_buffer_delete(img_buffer);
  //glDeleteTextures(1, &font_atlas->id);
  //texture_atlas_delete(font_atlas);
  if(unlink("gfx.fifo") == -1) { perror("GFX error: unlink"); exit(EXIT_FAILURE); }
  glfwDestroyWindow(t->window);
  glfwTerminate();
  dict_free(&t->cache);
  pthread_mutex_destroy(&t->cache_mutex);
  if(sem_destroy(&t->flush_pre) == -1) { perror("GFX error: sem_destroy"); exit(EXIT_FAILURE); }
  if(sem_destroy(&t->flush_post) == -1) { perror("GFX error: sem_destroy"); exit(EXIT_FAILURE); }
  if(sem_destroy(&t->should_quit) == -1) { perror("GFX error: sem_destroy"); exit(EXIT_FAILURE); }
  free(t);
  return EXIT_SUCCESS;
}
