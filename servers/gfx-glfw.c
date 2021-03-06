// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later.
// xxd -i < gfx-glfw_freetype-gl/v3f-t2f-c4f.vert > text.vert.xxd && xxd -i < gfx-glfw_freetype-gl/v3f-t2f-c4f.frag > text.frag.xxd && xxd -i < gfx-glfw.img.vert > img.vert.xxd && xxd -i < gfx-glfw.img.frag > img.frag.xxd && gcc -o gfx-glfw gfx-glfw.c gfx-glfw_freetype-gl/*.c $(pkg-config --libs --cflags x11 opengl glfw3 glew freetype2 MagickWand) -lpthread -lm && rm *.xxd && ./gfx-glfw
#define _GNU_SOURCE // for reallocarray on raspberry pi OS which has old libc
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
#include <wand/MagickWand.h> // raspberry OS version (due to old version?)
//#include <MagickWand/MagickWand.h> // arch linux version
#include <GL/glew.h>
#include "gfx-glfw_freetype-gl/freetype-gl.h"
#include "gfx-glfw_freetype-gl/mat4.h"
#include "gfx-glfw_freetype-gl/shader.h"
#include "gfx-glfw_freetype-gl/vertex-buffer.h"
#include <GLFW/glfw3.h>
#include "srr.h"
#include "gfx-util.h"
#include "evt-util.h"
#include "data-util.h"

static void glfw_error_callback(int error, const char * description) {
  fprintf(stderr, "GFX error: glfw_error_callback %s\n", description); exit(EXIT_FAILURE);
}

char * load_file(const char * path) {
  FILE * file = fopen(path, "rb"); if(!file) { perror("GFX/EVT error: fopen"); fprintf(stderr, "path %s\n", path); exit(EXIT_FAILURE); }

  fseek(file, 0L, SEEK_END);
  long size = ftell(file);
  rewind(file);

  char * buffer = malloc(size + 1); buffer[size] = '\0';
  if(!buffer) { fprintf(stderr, "GFX/EVT error: out of memory reading file %s", path); exit(EXIT_FAILURE); }

  if(fread(buffer, size, 1, file) != 1) { fprintf(stderr, "GFX/EVT error: failed to read %s", path); exit(EXIT_FAILURE); }

  fclose(file);
  return buffer;
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

static double add_text(vertex_buffer_t * buffer, double fw, double fh, texture_font_t * font, const char * text, vec4 * color, double x, double y) {
  size_t i;
  float r, g, b, a;
  if(color) { r = color->red; g = color->green; b = color->blue; a = color->alpha; }
  uint32_t prev = 0;
  while(*text) {
    uint32_t current = *text; // TODO the freetype-gl demos are pretty bad concerning codepoint/utf-8. They assume 1 byte per glyph.
    texture_glyph_t * glyph = texture_font_get_glyph(font, (const char *)&current);
    if(glyph) {
      float kerning = prev? texture_glyph_get_kerning(glyph, (const char *)&prev) : 0.0f;
      x += kerning * fw;
      if(buffer) {
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
      }
      x += glyph->advance_x * fw;
    }
    prev = current;
    text++;
  }
  return x;
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
  const char * evt_srr_path;
  const char * evt_mappings_path;
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
  pthread_mutex_t evt_mutex;
  bool held[K_COUNT];
  int pressed[K_COUNT];
  bool released[K_COUNT];
  double axis_and_triggers[6];
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

// Keyboard

static struct shared_amongst_thread_t * evt_callback_data = NULL;

int evt_translate(int glwf_key) {
  switch(glwf_key) {
    case GLFW_KEY_A: return K_A;
    case GLFW_KEY_B: return K_B;
    case GLFW_KEY_C: return K_C;
    case GLFW_KEY_D: return K_D;
    case GLFW_KEY_E: return K_E;
    case GLFW_KEY_F: return K_F;
    case GLFW_KEY_G: return K_G;
    case GLFW_KEY_H: return K_H;
    case GLFW_KEY_I: return K_I;
    case GLFW_KEY_J: return K_J;
    case GLFW_KEY_K: return K_K;
    case GLFW_KEY_L: return K_L;
    case GLFW_KEY_M: return K_M;
    case GLFW_KEY_N: return K_N;
    case GLFW_KEY_O: return K_O;
    case GLFW_KEY_P: return K_P;
    case GLFW_KEY_Q: return K_Q;
    case GLFW_KEY_R: return K_R;
    case GLFW_KEY_S: return K_S;
    case GLFW_KEY_T: return K_T;
    case GLFW_KEY_U: return K_U;
    case GLFW_KEY_V: return K_V;
    case GLFW_KEY_W: return K_W;
    case GLFW_KEY_X: return K_X;
    case GLFW_KEY_Y: return K_Y;
    case GLFW_KEY_Z: return K_Z;
    case GLFW_KEY_0: return K_N0;
    case GLFW_KEY_1: return K_N1;
    case GLFW_KEY_2: return K_N2;
    case GLFW_KEY_3: return K_N3;
    case GLFW_KEY_4: return K_N4;
    case GLFW_KEY_5: return K_N5;
    case GLFW_KEY_6: return K_N6;
    case GLFW_KEY_7: return K_N7;
    case GLFW_KEY_8: return K_N8;
    case GLFW_KEY_9: return K_N9;
    case GLFW_KEY_UP: return K_UP;
    case GLFW_KEY_DOWN: return K_DOWN;
    case GLFW_KEY_LEFT: return K_LEFT;
    case GLFW_KEY_RIGHT: return K_RIGHT;
    case GLFW_KEY_ESCAPE: return K_ESC;
    case GLFW_KEY_TAB: return K_TAB;
    case GLFW_KEY_LEFT_ALT: return K_ALT_L;
    case GLFW_KEY_RIGHT_ALT: return K_ALT_R;
    case GLFW_KEY_LEFT_CONTROL: return K_CTRL_L;
    case GLFW_KEY_RIGHT_CONTROL: return K_CTRL_R;
    case GLFW_KEY_LEFT_SHIFT: return K_SHIFT_L;
    case GLFW_KEY_RIGHT_SHIFT: return K_SHIFT_R;
    case GLFW_KEY_SPACE: return K_SPACE;
    case GLFW_KEY_F1: return K_F1;
    case GLFW_KEY_F2: return K_F2;
    case GLFW_KEY_F3: return K_F3;
    case GLFW_KEY_F4: return K_F4;
    case GLFW_KEY_F5: return K_F5;
    case GLFW_KEY_F6: return K_F6;
    case GLFW_KEY_F7: return K_F7;
    case GLFW_KEY_F8: return K_F8;
    case GLFW_KEY_F9: return K_F9;
    case GLFW_KEY_F10: return K_F10;
    case GLFW_KEY_F11: return K_F11;
    case GLFW_KEY_F12: return K_F12;
    case GLFW_KEY_ENTER: return K_ENTER;
    case GLFW_KEY_BACKSPACE: return K_BACKSPACE;
    case GLFW_KEY_PAGE_UP: return K_PGUP;
    case GLFW_KEY_PAGE_DOWN: return K_PGDOWN;
    case GLFW_KEY_GRAVE_ACCENT: return K_GRAVE;
    case GLFW_KEY_LEFT_BRACKET: return K_BRACE_L;
    case GLFW_KEY_RIGHT_BRACKET: return K_BRACE_R;
    case GLFW_KEY_PERIOD: return K_DOT;
    case GLFW_KEY_SEMICOLON: return K_SEMICOLON;
    case GLFW_KEY_APOSTROPHE: return K_APOSTROPHE;
    case GLFW_KEY_BACKSLASH: return K_BACKSLASH;
    case GLFW_KEY_SLASH: return K_SLASH;
    case GLFW_KEY_COMMA: return K_COMMA;
    default: return KEY_NONE;
  }
}

void evt_key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
  if(action == GLFW_REPEAT) return;
  key = evt_translate(key);
  if(key == KEY_NONE) return;

  if(pthread_mutex_lock(&evt_callback_data->evt_mutex)) { fprintf(stderr, "EVT error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
  if(action == GLFW_PRESS) {
    if(evt_callback_data->held[key]) printf("EVT warning: key %d was already held\n", key);
    evt_callback_data->held[key] = true;
    evt_callback_data->pressed[key]++;
  } else if(action == GLFW_RELEASE) {
    if(!evt_callback_data->held[key]) printf("EVT warning: key %d was already released\n", key);
    evt_callback_data->held[key] = false;
    evt_callback_data->released[key] = true;
  }
  if(pthread_mutex_unlock(&evt_callback_data->evt_mutex)) { fprintf(stderr, "EVT error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
}

// Mouse (not implemented)

void evt_mkey_callback(GLFWwindow * window, int button, int action, int mods) {
}

void evt_mskey_callback(GLFWwindow * window, double xoffset, double yoffset) {
}

// Joystick (I merge all joystick into gamepad 0)
// At this point in time, 2020-07-11, glfw does not have callback for joystick buttons, so I might miss presses when I poll.
// Issue is 5 years old: https://github.com/glfw/glfw/issues/601
// At this point in time, 2020-07-11, already connected devices do not generate connect callback on start, so I'd have to do it manually.
// This would open a short window where a connected device may get this callback twice or none at all.
// The issue was commented in https://github.com/glfw/glfw/issues/601
// In any case, I poll all joystick in the main loop after polling event, and merge the results in gamepad 0 so the issues are moot.

void evt_joystick_callback(int jid, int event) {
  if(event == GLFW_CONNECTED) {
    if(glfwJoystickIsGamepad(jid)) {
    }
  } else if(event == GLFW_DISCONNECTED) {
  }
}

void evt_joystick_poll(struct shared_amongst_thread_t * t) {
  const int base = G0_R1;
  const int N = 17;
  int held[N];
  memset(held, 0, N * sizeof(int));
  double axis[6];
  memset(axis, 0, 6 * sizeof(double));
  // poll each joystick
  for(int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++) {
    // glfw input guide says glfwJoystickIsGamepad can be used instead of glfwJoystickPresent, but it's a lie, you need both
    if(glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid)) {
      GLFWgamepadstate state;
      if(glfwGetGamepadState(jid, &state)) {
        held[G0_R1 - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
        held[G0_R2 - base] |= state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > .15;
        held[G0_R3 - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] == GLFW_PRESS;
        held[G0_L1 - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS;
        held[G0_L2 - base] |= state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > .15;
        held[G0_L3 - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] == GLFW_PRESS;
        held[G0_START - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
        held[G0_HOME - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_GUIDE] == GLFW_PRESS;
        held[G0_SELECT - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_BACK] == GLFW_PRESS;
        held[G0_NORTH - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_TRIANGLE] == GLFW_PRESS;
        held[G0_SOUTH - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_CROSS] == GLFW_PRESS;
        held[G0_EAST - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_CIRCLE] == GLFW_PRESS;
        held[G0_WEST - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_SQUARE] == GLFW_PRESS;
        held[G0_UP - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS;
        held[G0_DOWN - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS;
        held[G0_LEFT - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS;
        held[G0_RIGHT - base] |= state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS;
        axis[0] += state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
        axis[1] += state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        axis[2] += state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
        axis[3] += state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
        axis[4] += state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
        axis[5] += state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
      }
    }
  }

  if(pthread_mutex_lock(&t->evt_mutex)) { fprintf(stderr, "EVT error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
  // update buttons
  for(int k = G0_R1; k <= G0_RIGHT; k++) {
    bool was_held = t->held[k];
    bool is_held = held[k - base];
    if(was_held && !is_held) t->released[k] = true;
    if(!was_held && is_held) t->pressed[k]++;
    t->held[k] = is_held;
  }
  // update axis_and_triggers
  for(int i = 0; i < 6; i++) {
    if(axis[i] > 1) axis[i] = 1;
    if(axis[i] < -1) axis[i] = -1;
    t->axis_and_triggers[i] = axis[i];
  }
  if(pthread_mutex_unlock(&t->evt_mutex)) { fprintf(stderr, "EVT error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
}

static void flush_font_buffer(GLuint shader, mat4 * model, mat4 * projection, texture_atlas_t * atlas, vertex_buffer_t * buffer) {
  glUseProgram(shader);
  glUniform1i(glGetUniformLocation(shader, "texture"), 0);
  glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, model->data);
  glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, projection->data);
  glBindTexture(GL_TEXTURE_2D, atlas->id);
  // upload atlas if it changed
  if(atlas->dirty) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas->width, atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, atlas->data);
    atlas->dirty = 0;
  }
  vertex_buffer_render(buffer, GL_TRIANGLES);
  vertex_buffer_clear(buffer);
}

static void flush_img_buffer(GLuint shader, mat4 * model, mat4 * projection, GLuint tex_id, vertex_buffer_t * buffer) {
  //printf("flush_img_buffer\n");
  glUseProgram(shader);
  glUniform1i(glGetUniformLocation(shader, "my_sampler"), 0);
  glEnable(GL_TEXTURE_2D);
  glActiveTexture(GL_TEXTURE0);
  glUniformMatrix4fv(glGetUniformLocation(shader, "my_model"), 1, 0, model->data);
  glUniformMatrix4fv(glGetUniformLocation(shader, "my_projection"), 1, 0, projection->data);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  vertex_buffer_render(buffer, GL_TRIANGLES);
  vertex_buffer_clear(buffer);
}

static void * handle_fifo_loop(void * vargp) {
  // this is the 'main' thread as far as glfw is concerned
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
  int img_buffer_texture_id = 0;
  texture_atlas_t * font_buffer_atlas = NULL;
  int old_w = 0, old_h = 0;
  int line_buffer_capacity = 1;
  char ** lines_ptr = reallocarray(NULL, line_buffer_capacity, sizeof(char *));
  int * line_widths = reallocarray(NULL, line_buffer_capacity, sizeof(int));

  // matrices
  mat4 projection;
  mat4_set_identity(&projection);
  const int stack_capacity = 20;
  int stack_index = 0;
  mat4 model[stack_capacity];
  mat4_set_identity(&model[stack_index]);

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
          // show window
          bool no_pref = t->preferred_W == 0;
          if(no_pref) {
            GLFWmonitor * monitor = glfwGetPrimaryMonitor();
            if(monitor) {
              const GLFWvidmode * mode = glfwGetVideoMode(monitor);
              glfwSetWindowMonitor(t->window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
          } else {
            glfwSetWindowMonitor(t->window, NULL, 0, 0, t->preferred_W, t->preferred_H, 0);
          }
          glfwShowWindow(t->window);
          // evt-listener
          evt_callback_data = t;
          if(t->evt_mappings_path) {
            char * mappings = load_file(t->evt_mappings_path);
            glfwUpdateGamepadMappings(mappings);
            free(mappings);
          }
          glfwSetKeyCallback(t->window, evt_key_callback);
          glfwSetMouseButtonCallback(t->window, evt_mkey_callback);
          glfwSetScrollCallback(t->window, evt_mskey_callback);
          glfwSetJoystickCallback(evt_joystick_callback);
        }
        // on flush, stop handling any more messages until srr thread completes the flush
        if(sem_post(&t->flush_pre) == -1) { perror("GFX error: sem_post"); exit(EXIT_FAILURE); }
        if(sem_wait(&t->flush_post) == -1) { perror("GFX error: sem_wait"); exit(EXIT_FAILURE); }
        // flush our drawing to the screen
        if(t->running) {
          if(vertex_buffer_size(img_buffer)) flush_img_buffer(img_shader, &model[stack_index], &projection, img_buffer_texture_id, img_buffer);
          if(vertex_buffer_size(font_buffer)) flush_font_buffer(font_shader, &model[stack_index], &projection, font_buffer_atlas, font_buffer);
          glfwSwapBuffers(t->window);
          glfwPollEvents();
          evt_joystick_poll(t);
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
        if(t->window) glfwSetWindowTitle(t->window, &line[6]);
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
        if(!glfwInit()) { fprintf(stderr, "GFX error: glfwInit\n"); exit(EXIT_FAILURE); }
        printf("GFX create window\n");
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
        t->window = glfwCreateWindow(t->W, t->H, title? title : NULL, NULL, NULL); if(!t->window) { fprintf(stderr, "GFX error: glfwCreateWindow\n"); exit(EXIT_FAILURE); }
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
        if(vertex_buffer_size(font_buffer)) flush_font_buffer(font_shader, &model[stack_index], &projection, font_buffer_atlas, font_buffer);
        char * line_sep = &line[5];
        // normal: draw path x y
        const char * path = strsep(&line_sep, " ");
        if(pthread_mutex_lock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
        if(dict_has(&t->cache, path)) {
          struct res * res = dict_get(&t->cache, path);
          if(vertex_buffer_size(img_buffer) && res->texture != img_buffer_texture_id) flush_img_buffer(img_shader, &model[stack_index], &projection, img_buffer_texture_id, img_buffer);
          img_buffer_texture_id = res->texture;
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
                  { p5, p6,            (p1) / res->w, (p2) / res->h },
                  { p5, p6 + p4,       (p1) / res->w, (p2 + p4) / res->h },
                  { p5 + p3, p6 + p4,  (p1 + p3) / res->w, (p2 + p4) / res->h },
                  { p5 + p3, p6,       (p1 + p3) / res->w, (p2) / res->h }
                };
                vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
              } else {
                const char * tmp = strsep(&line_sep, " ");
                if(strcmp(tmp, "mx") == 0) {
                  struct { float x, y; float s, t; } vertices[4] = {
                    { p5, p6,            (p1 + p3) / res->w, (p2) / res->h },
                    { p5, p6 + p4,       (p1 + p3) / res->w, (p2 + p4) / res->h },
                    { p5 + p3, p6 + p4,  (p1) / res->w, (p2 + p4) / res->h },
                    { p5 + p3, p6,       (p1) / res->w, (p2) / res->h }
                  };
                  vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
                } else {
                  // region: draw path sx sy sw sh x y w h (mx=mirror-x) (texture_offset_x)
                  double p7 = strtod(tmp, NULL);
                  double p8 = strtod(strsep(&line_sep, " "), NULL);
                  bool mx = false;
                  double texture_offset_x = 0;
                  if(line_sep) {
                    tmp = strsep(&line_sep, " ");
                    mx = strcmp(tmp, "mx") == 0;
                    if(!mx) texture_offset_x = strtod(tmp, NULL);
                    else if(line_sep) texture_offset_x = strtod(strsep(&line_sep, " "), NULL);
                  }
                  if(mx) {
                    struct { float x, y; float s, t; } vertices[4] = {
                      { p5, p6,            (p1 + p3) / res->w, (p2) / res->h },
                      { p5, p6 + p8,       (p1 + p3) / res->w, (p2 + p4) / res->h },
                      { p5 + p7, p6 + p8,  (p1) / res->w, (p2 + p4) / res->h },
                      { p5 + p7, p6,       (p1) / res->w, (p2) / res->h }
                    };
                    vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
                  } else {
                    struct { float x, y; float s, t; } vertices[4] = {
                      { p5, p6,            (p1) / res->w, (p2) / res->h },
                      { p5, p6 + p8,       (p1) / res->w, (p2 + p4) / res->h },
                      { p5 + p7, p6 + p8,  (p1 + p3) / res->w, (p2 + p4) / res->h },
                      { p5 + p7, p6,       (p1 + p3) / res->w, (p2) / res->h }
                    };
                    vertex_buffer_push_back(img_buffer, vertices, 4, indices, 6);
                  }
                }
              }
            }
          }
        }
        if(pthread_mutex_unlock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
      } else if(starts_with(line, "text ")) {
        if(vertex_buffer_size(img_buffer)) flush_img_buffer(img_shader, &model[stack_index], &projection, img_buffer_texture_id, img_buffer);
        char * line_sep = &line[5];
        const char * path = strsep(&line_sep, " ");
        if(pthread_mutex_lock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
        if(dict_has(&t->cache, path)) {
          // text font x y w h valign halign line_count clip scroll outline_color fill_color outline_size message
          struct res * res = dict_get(&t->cache, path);
          if(vertex_buffer_size(font_buffer) && font_buffer_atlas != res->atlas) flush_font_buffer(font_shader, &model[stack_index], &projection, font_buffer_atlas, font_buffer);
          font_buffer_atlas = res->atlas;
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
          double line_height = h / line_count;
          vec4 outline = {{outline_r,outline_g,outline_b,outline_a}};
          vec4 fill = {{fill_r,fill_g,fill_b,fill_a}};
          
          // font size
          texture_font_t * font;
          double font_size = line_height * t->aspectH / t->H;
          int font_key = (int)(font_size * 1000);
          if(dict_has(res->fonts, font_key)) {
            font = dict_get(res->fonts, font_key);
          } else {
            font = texture_font_new_from_file(res->atlas, font_size, path);
            dict_set(res->fonts, font_key, font);
          }
          
          // line break
          font->outline_thickness = outline_size * t->aspectH / t->H;
          font->rendermode = RENDER_OUTLINE_EDGE;
          double fw = t->W / (double)t->aspectW;
          double fh = t->H / (double)t->aspectH;
          char * message = line_sep;
          if(message) {
            int line_ptr_count = 0;
            while(*message) {
              // store start of line
              if(line_ptr_count == line_buffer_capacity) {
                line_buffer_capacity *= 2;
                lines_ptr = reallocarray(lines_ptr, line_buffer_capacity, sizeof(char *));
                line_widths = reallocarray(line_widths, line_buffer_capacity, sizeof(int));
              }
              lines_ptr[line_ptr_count] = message;
              int line_width = 0;
              char * search = message;
              char * good_end = search;
              // TODO assumes w != 0
              while(line_width <= w) {
                good_end = search;
                line_widths[line_ptr_count] = line_width;
                // did we reach end of line
                if(*search == '\0') break;
                if(starts_with(search, "\\n")) break;
                if(*search == ' ') search++;
                // find next space, \\n or \0
                while(*search != ' ' && *search != '\0' && !starts_with(search, "\\n")) search++;
                char stored = *search; *search = '\0';
                // measure
                line_width = add_text(NULL, fw, fh, font, message, NULL, 0, 0);
                *search = stored;
                // did the first word bust? we did our best.
                if(line_width > w && good_end == message) {
                  good_end = search;
                  line_widths[line_ptr_count] = line_width;
                }
              }
              // did we reach end of line
              if(*good_end == '\0') message = good_end;
              if(*good_end == ' ') message = good_end + 1;
              if(starts_with(good_end, "\\n")) message = good_end + 2;
              *good_end = '\0';
              line_ptr_count++;
            }

            // render
            if(str_equals(valign, "bottom")) y += h - line_ptr_count * line_height;
            else if(str_equals(valign, "center")) y += (h - line_ptr_count * line_height) / 2;
            for(int i = 0; i < line_ptr_count; i++) {
              double tx = x + outline_size;
              if(str_equals(halign, "right")) tx += w - line_widths[i] - 1 - 2 * outline_size;
              else if(str_equals(halign, "center")) tx += (w - line_widths[i] - 1 - outline_size) / 2;
              font->rendermode = RENDER_OUTLINE_NEGATIVE;
              add_text(font_buffer, fw, fh, font, lines_ptr[i], &fill, tx, y + line_height);
              font->rendermode = RENDER_OUTLINE_EDGE;
              add_text(font_buffer, fw, fh, font, lines_ptr[i], &outline, tx, y + line_height);
              y += line_height;
            }
          }
        }
        if(pthread_mutex_unlock(&t->cache_mutex)) { fprintf(stderr, "GFX error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
      } else if(starts_with(line, "fill ")) {
        if(vertex_buffer_size(img_buffer)) flush_img_buffer(img_shader, &model[stack_index], &projection, img_buffer_texture_id, img_buffer);
        if(vertex_buffer_size(font_buffer)) flush_font_buffer(font_shader, &model[stack_index], &projection, font_buffer_atlas, font_buffer);
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
        glUniformMatrix4fv(glGetUniformLocation(fill_shader, "my_model"), 1, 0, model[stack_index].data);
        glUniformMatrix4fv(glGetUniformLocation(fill_shader, "my_projection"), 1, 0, projection.data);
        GLuint indices[6] = {0,1,2, 0,2,3};
        struct { float x, y; } vertices[4] = {
          { x,y },
          { x,y+h },
          { x+w,y+h },
          { x+w,y }
        };
        vertex_buffer_push_back(fill_buffer, vertices, 4, indices, 6);
        // TODO to do batch draw for color, I'll first need to put the color in the vertex array
        vertex_buffer_render(fill_buffer, GL_TRIANGLES);
        vertex_buffer_clear(fill_buffer);
      } else if(starts_with(line, "push ")) {
        if(vertex_buffer_size(img_buffer)) flush_img_buffer(img_shader, &model[stack_index], &projection, img_buffer_texture_id, img_buffer);
        if(vertex_buffer_size(font_buffer)) flush_font_buffer(font_shader, &model[stack_index], &projection, font_buffer_atlas, font_buffer);
        stack_index++;
        if(stack_index == stack_capacity) { fprintf(stderr, "stack push too far\n"); exit(EXIT_FAILURE); }
        memcpy(&model[stack_index], &model[stack_index - 1], sizeof(mat4));
        
        char * line_sep = &line[5];
        const char * transform = strsep(&line_sep, " ");
        if(str_equals(transform, "rotate")) {
          double x = strtod(strsep(&line_sep, " "), NULL);
          double y = strtod(strsep(&line_sep, " "), NULL);
          double theta = strtod(strsep(&line_sep, " "), NULL);
          double angle_in_degrees = theta * 180 / M_PI;
          mat4_translate(&model[stack_index], -x, -y, 0);
          mat4_rotate(&model[stack_index], angle_in_degrees, 0, 0, 1);
          mat4_translate(&model[stack_index], x, y, 0);
        } else if(str_equals(transform, "scale")) {
          double x = strtod(strsep(&line_sep, " "), NULL);
          double y = strtod(strsep(&line_sep, " "), NULL);
          double sh = strtod(strsep(&line_sep, " "), NULL);
          double sv = strtod(strsep(&line_sep, " "), NULL);
          mat4_translate(&model[stack_index], -x, -y, 0);
          mat4_scale(&model[stack_index], sh, sv, 1);
          mat4_translate(&model[stack_index], x, y, 0);
        }
      } else if(str_equals(line, "pop")) {
        if(stack_index == 0) { fprintf(stderr, "stack pop too far\n"); exit(EXIT_FAILURE); }
        if(vertex_buffer_size(img_buffer)) flush_img_buffer(img_shader, &model[stack_index], &projection, img_buffer_texture_id, img_buffer);
        if(vertex_buffer_size(font_buffer)) flush_font_buffer(font_shader, &model[stack_index], &projection, font_buffer_atlas, font_buffer);
        stack_index--;
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
  double t0;
  uint64_t monotonic_time_ms;
  uint64_t monotonic_time_ms_prev;
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
    if(t->first_flush) {
      t->first_flush = false;
      t0 = glfwGetTime();
      monotonic_time_ms_prev = 0;
    }
    double t1 = glfwGetTime();
    monotonic_time_ms = (t1 - t0) * 1000;
    // let fifo resume
    if(sem_post(&t->flush_post) == -1) { perror("GFX error: sem_post"); exit(EXIT_FAILURE); }
    // parse commands
    char * buffer = _buffer;
    char * command;
    while(command = strsep(&buffer, " ")) {
      if(str_equals(command, "delta")) {
        mem->msg[i++] = GFX_STAT_DLT;
        *((int *)(&mem->msg[i])) = (int)(monotonic_time_ms - monotonic_time_ms_prev); i+=4;
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
    monotonic_time_ms_prev = monotonic_time_ms;
    // reply
    error = srr_reply(&server, i); if(error) { fprintf(stderr, "GFX error: srr_reply: %s\n", error); exit(EXIT_FAILURE); }
    if(!t->running) break;
  }

  error = srr_disconnect(&server); if(error) { fprintf(stderr, "GFX error: srr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
  if(sem_post(&t->should_quit) == -1) { perror("GFX error: sem_post"); exit(EXIT_FAILURE); }
  return NULL;
}

static void * handle_evt_srr_loop(void * vargp) {
  struct shared_amongst_thread_t * t = vargp;

  // connect
  printf("EVT starting srr %s\n", t->evt_srr_path);
  const char * error;
  struct srr server;
  error = srr_init(&server, t->evt_srr_path, 8192, true, false, 3); if(error) { printf("EVT error: srr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct srr_direct * mem = srr_direct(&server);

  // wait for a message from the client
  char _buffer[8193];
  while(1) {
    // receive
    error = srr_receive(&server); if(error) { printf("EVT error: srr_receive: %s\n", error); exit(EXIT_FAILURE); }
    // copy message, because the reply will overwrite it as we parse
    memcpy(_buffer, mem->msg, mem->length);
    _buffer[mem->length] = '\0';
    // read commands
    char * buffer = _buffer;
    char * command;
    bool joystick_only = false;
    while(command = strsep(&buffer, " ")) {
      if(str_equals(command, "no-focus-mode")) {
        joystick_only = true;
      }
    }
    // build reply
    int i = 0;
    if(pthread_mutex_lock(&t->evt_mutex)) { fprintf(stderr, "EVT error: pthread_mutex_lock\n"); exit(EXIT_FAILURE); }
    // keys/buttons (1 x keyboard, 4 x mouse, 4 x gamepads) 4 bits format (held:1 press_count:2 released:1)
    // memset(mem->msg + i, 0, K_COUNT/2); i += K_COUNT/2;
    for(int k = 0; k < K_COUNT / 2; k++) {
      int index_a = 2 * k;
      int index_b = index_a + 1;
      if(t->pressed[index_a] > 3) t->pressed[index_a] = 3;
      if(t->pressed[index_b] > 3) t->pressed[index_b] = 3;
      mem->msg[i++] =
        (t->held[index_a] << 7) | (t->pressed[index_a] << 5) | (t->released[index_a] << 4) |
        (t->held[index_b] << 3) | (t->pressed[index_b] << 1) | t->released[index_b];
      t->pressed[index_a] = t->pressed[index_b] = 0;
      t->released[index_a] = t->released[index_b] = 0;
    }
    // 4 x mouse 3 bytes format (dx, dy, dw)
    memset(mem->msg + i, 0, 3); i += 3;
    memset(mem->msg + i, 0, 3); i += 3;
    memset(mem->msg + i, 0, 3); i += 3;
    memset(mem->msg + i, 0, 3); i += 3;
    // 4 x gamepad axis and triggers 6 bytes format (lx, ly, rx, ry, lt, rt)
    //memset(mem->msg + i, 0, 6); i += 6;
    mem->msg[i++] = t->axis_and_triggers[0] * 127;
    mem->msg[i++] = t->axis_and_triggers[1] * 127;
    mem->msg[i++] = t->axis_and_triggers[2] * 127;
    mem->msg[i++] = t->axis_and_triggers[3] * 127;
    mem->msg[i++] = t->axis_and_triggers[4] * 255;
    mem->msg[i++] = t->axis_and_triggers[5] * 255;
    memset(mem->msg + i, 0, 6); i += 6;
    memset(mem->msg + i, 0, 6); i += 6;
    memset(mem->msg + i, 0, 6); i += 6;
    // histo key 16 bytes (key ids) followed by 16 bits (press/release)
    memset(mem->msg + i, KEY_NONE, 16); i += 16;
    mem->msg[i++] = 0;
    mem->msg[i++] = 0;
    if(pthread_mutex_unlock(&t->evt_mutex)) { fprintf(stderr, "EVT error: pthread_mutex_unlock\n"); exit(EXIT_FAILURE); }
    // reply
    error = srr_reply(&server, i); if(error) { fprintf(stderr, "EVT error: srr_reply: %s\n", error); exit(EXIT_FAILURE); }
    if(!t->running) break;
  }

  error = srr_disconnect(&server); if(error) { fprintf(stderr, "EVT error: srr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
  if(sem_post(&t->should_quit) == -1) { perror("EVT error: sem_post"); exit(EXIT_FAILURE); }
  return NULL;
}

int main(int argc, char** argv) {
  struct shared_amongst_thread_t * t = calloc(1, sizeof(struct shared_amongst_thread_t)); // place it on heap, as it is unclear is threads can properly access stack var
  t->running = true;
  t->first_flush = true;
  t->focused = true;
  dict_init(&t->cache, sizeof(struct res), true, true);
  if(pthread_mutex_init(&t->cache_mutex, NULL) != 0) { fprintf(stderr, "GFX error: pthread_mutex_init\n"); exit(EXIT_FAILURE); }
  if(pthread_mutex_init(&t->evt_mutex, NULL) != 0) { fprintf(stderr, "EVT error: pthread_mutex_init\n"); exit(EXIT_FAILURE); }
  if(sem_init(&t->flush_pre, 0, 0) == -1) { perror("GFX error: sem_init"); exit(EXIT_FAILURE); }
  if(sem_init(&t->flush_post, 0, 0) == -1) { perror("GFX error: sem_init"); exit(EXIT_FAILURE); }
  if(sem_init(&t->should_quit, 0, 0) == -1) { perror("GFX error: sem_init"); exit(EXIT_FAILURE); }
  t->evt_mappings_path = argc < 4? NULL : argv[3];

  // create fifo and in another thread read its messages, this is the 'main' thread as far as glfw is concerned (for evt too)
  printf("GFX fifo\n");
  if(unlink("gfx.fifo") == -1 && errno != ENOENT) { perror("GFX error: unlink"); exit(EXIT_FAILURE); }
  if(mkfifo("gfx.fifo", S_IRUSR | S_IWUSR) == -1) { perror("GFX error: mkfifo"); exit(EXIT_FAILURE); }
  pthread_t fifo_thread;
  pthread_create(&fifo_thread, NULL, handle_fifo_loop, t);

  // create srr server in another thread and listen to messages
  printf("GFX srr\n");
  if(argc < 2) { fprintf(stderr, "GFX error: must specify the srr shm name as arg\n"); exit(EXIT_FAILURE); }
  t->srr_path = argv[1];
  pthread_t srr_thread;
  pthread_create(&srr_thread, NULL, handle_srr_loop, t);

  // create evt srr server in another thread and listen to messages
  printf("EVT srr\n");
  if(argc < 3) { printf("GFX/EVT not using evt in glfw solution.\n"); }
  else {
    t->evt_srr_path = argv[2];
    pthread_t evt_srr_thread;
    pthread_create(&evt_srr_thread, NULL, handle_evt_srr_loop, t);
  }

  // wait for signal to quit
  if(sem_wait(&t->should_quit) == -1) { perror("GFX/EVT error: sem_wait"); exit(EXIT_FAILURE); }

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
  pthread_mutex_destroy(&t->evt_mutex);
  if(sem_destroy(&t->flush_pre) == -1) { perror("GFX error: sem_destroy"); exit(EXIT_FAILURE); }
  if(sem_destroy(&t->flush_post) == -1) { perror("GFX error: sem_destroy"); exit(EXIT_FAILURE); }
  if(sem_destroy(&t->should_quit) == -1) { perror("GFX/EVT error: sem_destroy"); exit(EXIT_FAILURE); }
  free(t);
  return EXIT_SUCCESS;
}
