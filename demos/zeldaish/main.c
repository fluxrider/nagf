// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o zeldaish main.c ../../utils/*.c -L../../libsrr -I../../libsrr -I../../utils -lsrr $(pkg-config --libs --cflags libxml-2.0) -lm
// LD_LIBRARY_PATH=../../libsrr ./zeldaish
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <math.h>
#include "srr.h"
#include "evt-util.h"
#include "gfx-util.h"
#include "data-util.h"

// NOTES: cane / elf / key / chest / bottle / fountain / fire / staff / wizard / spell / dragon / heart

uint64_t currentTimeMillis() {
  struct timespec tp;
  if(clock_gettime(CLOCK_MONOTONIC, &tp) == -1) { perror("read"); exit(1); }
  return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

struct tile_animation {
  int size;
  int * ids;
  uint64_t * durations;
  uint64_t total_duration;
};

void main(int argc, char * argv[]) {
  // connect
  const char * error;
  struct srr evt;
  struct srr gfs;
  error = srr_init(&evt, "/zeldaish-evt", 8192, false, false, 3); if(error) { printf("srr_init(evt): %s\n", error); exit(EXIT_FAILURE); }
  error = srr_init(&gfs, "/zeldaish-gfx", 8192, false, false, 3); if(error) { printf("srr_init(gfs): %s\n", error); exit(EXIT_FAILURE); }
  struct srr_direct * emm = srr_direct(&evt);
  struct srr_direct * gmm = srr_direct(&gfs);
  int gfx = open("gfx.fifo", O_WRONLY); if(gfx == -1) { perror("open(gfx.fifo)"); exit(EXIT_FAILURE); }
  int snd = open("snd.fifo", O_WRONLY); if(snd == -1) { perror("open(snd.fifo)"); exit(EXIT_FAILURE); }

  // parse map created with Tiled (https://www.mapeditor.org/)
  struct dict animated_tiles;
  struct dict blocking_tiles;
  dict_init(&animated_tiles, sizeof(struct tile_animation), false, false);
  dict_init(&blocking_tiles, 0, false, false);
  xmlChar * tileset_image = NULL;
  int tilewidth;
  int tileheight;
  int tileset_columns;
  const int layers_capacity = 2;
  const int map_col = 16;
  const int map_row = 11;
  int layers[layers_capacity][map_row][map_col];
  int layers_size = 0;
  xmlDoc * map = xmlParseFile("fountain.tmx"); if(!map) { printf("xmlParseFile(fountain.tmx) failed.\n"); exit(EXIT_FAILURE); }
  xmlNode * mcur = xmlDocGetRootElement(map); if(!mcur) { printf("xmlDocGetRootElement() is null.\n"); exit(EXIT_FAILURE); }
  mcur = mcur->xmlChildrenNode;
  while(mcur != NULL) {
    // load tileset
    if(xmlStrcmp(mcur->name, "tileset") == 0) {
      xmlChar * source = xmlGetProp(mcur, "source");
      xmlDoc * tileset = xmlParseFile(source); if(!tileset) { printf("xmlParseFile(%s) failed.\n", source); exit(EXIT_FAILURE); }
      xmlNode * tcur = xmlDocGetRootElement(tileset); if(!tcur) { printf("xmlDocGetRootElement() is null.\n"); exit(EXIT_FAILURE); }
      xmlFree(source);
      xmlChar * str_tilewidth = xmlGetProp(tcur, "tilewidth");
      xmlChar * str_tileheight = xmlGetProp(tcur, "tileheight");
      xmlChar * str_columns = xmlGetProp(tcur, "columns");
      tilewidth = strtol(str_tilewidth, NULL, 10);
      tileheight = strtol(str_tileheight, NULL, 10);
      tileset_columns = strtol(str_columns, NULL, 10);
      xmlFree(str_tilewidth);
      xmlFree(str_tileheight);
      xmlFree(str_columns);
      tcur = tcur->xmlChildrenNode;
      while(tcur != NULL) {
        if(xmlStrcmp(tcur->name, "image") == 0) {
          tileset_image = xmlGetProp(tcur, "source");
        }
        else if(xmlStrcmp(tcur->name, "tile") == 0) {
          xmlChar * id = xmlGetProp(tcur, "id");
          // store blocking tiles
          xmlChar * type = xmlGetProp(tcur, "type");
          if(type && xmlStrcmp(type, "block") == 0) dict_set(&blocking_tiles, strtol(id, NULL, 10), true);
          xmlFree(type);
          // store animations
          xmlNode * acur = tcur->xmlChildrenNode;
          while(acur != NULL) {
            if(xmlStrcmp(acur->name, "animation") == 0) {
              // count how many frames
              struct tile_animation anim = {0};
              xmlNode * fcur = acur->xmlChildrenNode;
              while(fcur != NULL) {
                if(xmlStrcmp(fcur->name, "frame") == 0) anim.size++;
                fcur = fcur->next;
              }
              // alloc and store in dictionary
              anim.ids = malloc(sizeof(int) * anim.size);
              anim.durations = malloc(sizeof(uint64_t) * anim.size);
              // populate ids/durations
              fcur = acur->xmlChildrenNode;
              int i = 0;
              while(fcur != NULL) {
                if(xmlStrcmp(fcur->name, "frame") == 0) {
                  xmlChar * t = xmlGetProp(fcur, "tileid");
                  xmlChar * d = xmlGetProp(fcur, "duration");
                  anim.ids[i] = strtol(t, NULL, 10);
                  anim.durations[i] = strtol(d, NULL, 10);
                  anim.total_duration += anim.durations[i];
                  i++;
                  xmlFree(d);
                  xmlFree(t);
                }
                fcur = fcur->next;
              }
              dict_set(&animated_tiles, strtol(id, NULL, 10), (intptr_t)&anim);
            }
            acur = acur->next;
          }
          xmlFree(id);
        }
        tcur = tcur->next;
      }
    }
    // layers
    else if(xmlStrcmp(mcur->name, "layer") == 0) {
      xmlNode * node = mcur->xmlChildrenNode;
      while(node != NULL) {
        if(xmlStrcmp(node->name, "data") == 0) {
          if(layers_size == layers_capacity) { printf("layers array full\n"); exit(EXIT_FAILURE); }
          int index = layers_size++;
          xmlChar * data = xmlNodeListGetString(map, node->xmlChildrenNode, 1);
          char * p = data;
          int row = 0, col = 0;
          while(*p) {
            // change row
            if(*p == '\n' && col > 0) {
              row++;
              col = 0;
              p++;
            } else {
              char * end;
              int tile = strtol(p, &end, 10);
              // if the number is valid, store it
              if(end != p) {
                if(row >= map_row) { printf("too many row in map data\n"); exit(EXIT_FAILURE); }
                if(col >= map_col) { printf("too many col in map data\n"); exit(EXIT_FAILURE); }
                layers[index][row][col++] = tile;
                p = end;
              }
              // if it wasn't a number, skip over
              else { p++; }
            }
          }
          xmlFree(data);
        }
        node = node->next;
      }
    }
    mcur = mcur->next;
  }
  xmlFreeDoc(map);
  if(!tileset_image) { printf("did not find anything tileset image while parsing map\n"); exit(EXIT_FAILURE); }

  // game setup
  dprintf(snd, "stream bg1.ogg\n");
  dprintf(gfx, "title %s\n", argv[0]);
  dprintf(gfx, "window 256 224\n");
  dprintf(gfx, "cache %s\n", tileset_image);
  dprintf(gfx, "cache princess.png\n");

  // game loop
  bool running = true;
  bool focused = true;
  bool loading = true;
  double px = 0;
  double py = 0;
  double delta_time = 0;
  double step_per_seconds = 100;
  int facing_index = 0;
  bool facing_mirror = false;
  while(running) {
    // input
    sprintf(emm->msg, focused? "" : "no-focus-mode"); error = srr_send(&evt, strlen(emm->msg)); if(error) { printf("srr_send(evt): %s\n", error); exit(EXIT_FAILURE); }
    struct evt_axis_and_triggers_normalized axis = evt_deadzoned(evt_axis_and_triggers(&evt, 0), .2, .2);
    running &= !evt_released(&evt, K_ESC);
    // walking
    if(evt_held(&evt, G0_DOWN) || evt_held(&evt, K_S)) axis.ly = fmin(1, axis.ly + 1);
    if(evt_held(&evt, G0_UP) || evt_held(&evt, K_W)) axis.ly = fmax(-1, axis.ly - 1);
    if(evt_held(&evt, G0_RIGHT) || evt_held(&evt, K_D)) axis.lx = fmin(1, axis.lx + 1);
    if(evt_held(&evt, G0_LEFT) || evt_held(&evt, K_A)) axis.lx = fmax(-1, axis.lx - 1);
    px += delta_time * step_per_seconds * axis.lx;
    py += delta_time * step_per_seconds * axis.ly;
    if(axis.lx != 0 || axis.ly != 0) {
      facing_index = (fabs(axis.ly) > fabs(axis.lx))? ((axis.ly < 0)? 2 : 0) : 1;
      facing_mirror = fabs(axis.ly) <= fabs(axis.lx) && axis.lx < 0;
    }

    // gfx
    if(!loading) {
      // draw tilemap
      for(int i = 0; i < layers_size; i++) {
        for(int row = 0; row < map_row; row++) {
          for(int col = 0; col < map_col; col++) {
            int tile = layers[i][row][col];
            if(tile != 0) {
              tile = tile - 1;
              // handle animated tiles
              struct tile_animation * anim = dict_get(&animated_tiles, tile);
              if(anim) {
                // TODO tick based, not time based
                uint64_t t = currentTimeMillis() % anim->total_duration;
                for(int i = 0; i < anim->size; i++) {
                  if(t < anim->durations[i]) { tile = anim->ids[i]; break; }
                  t-= anim->durations[i];
                }
              }
              int x = tilewidth * col;
              int y = tileheight * (row + 3); // +3 for the hud
              int tx = tilewidth * (tile % tileset_columns);
              int ty = tileheight * (tile / tileset_columns);
              dprintf(gfx, "draw %s %d %d 16 16 %d %d\n", tileset_image, tx, ty, x, y);
            }
          }
        }
      }
      dprintf(gfx, "draw princess.png %d %d 14 24 %f %f %s\n", 0, facing_index * 24, px, py, facing_mirror? "mx" : "");
    }
    // draw player
    dprintf(gfx, "flush\n");
    sprintf(gmm->msg, "flush delta stat %s", tileset_image); error = srr_send(&gfs, strlen(gmm->msg)); if(error) { printf("srr_send(gfs): %s\n", error); exit(EXIT_FAILURE); }
    focused = gmm->msg[0];
    running &= !gmm->msg[1];
    int i = 10;
    if(gmm->msg[i++] != GFX_STAT_DLT) { printf("unexpected stat result, wanted delta time\n"); exit(EXIT_FAILURE); }
    delta_time = *(int *)&gmm->msg[i] / 1000.0; i+= 4;
    //printf("%d\n", (int)(delta_time * 1000));
    if(gmm->msg[i] == GFX_STAT_ERR) { printf("stat error %c%c%c\n", gmm->msg[i+1], gmm->msg[i+2], gmm->msg[i+3]); exit(EXIT_FAILURE); }
    if(gmm->msg[i++] != GFX_STAT_IMG) { printf("unexpected stat result, wanted img\n"); exit(EXIT_FAILURE); }
    int w = *(int *)&gmm->msg[i]; i+= 4;
    int h = *(int *)&gmm->msg[i]; i+= 4;
    loading = w == 0;
    if(loading) { printf("loading progress %f\n", h / 1000.0); }
  }

  // disconnect
  dict_free(&blocking_tiles);
  dict_free(&animated_tiles);
  xmlFree(tileset_image);
  close(snd);
  close(gfx);
  error = srr_disconnect(&gfs); if(error) { printf("srr_disconnect(gfx): %s\n", error); exit(EXIT_FAILURE); }
  error = srr_disconnect(&evt); if(error) { printf("srr_disconnect(evt): %s\n", error); exit(EXIT_FAILURE); }
}
