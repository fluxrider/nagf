// Copyright 2020 David Lareau. This program is free software under the terms of the Zero Clause BSD.
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
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <math.h>
#include "srr.h"
#include "evt-util.h"
#include "gfx-util.h"
#include "data-util.h"

// NOTES: cane / elf / key / chest / bottle / fountain / fire / staff / wizard / spell / dragon / heart

struct tile_animation {
  int size;
  int * ids;
  uint64_t * durations;
  uint64_t total_duration;
};

struct map_node {
  const char * filename;
  struct map_node * north;
  struct map_node * south;
  struct map_node * east;
  struct map_node * west;
};

struct rect {
  double x;
  double y;
  double w;
  double h;
};

bool collides_1D(double p, double pl, double q, double ql) {
  return p + pl >= q && p <= q + ql;
}

bool collides_2D(struct rect * p, struct rect * q) {
  return collides_1D(p->x, p->w, q->x, q->w) && collides_1D(p->y, p->h, q->y, q->h);
}

bool collides_2D_dx(double px, double py, double pw, double ph, struct rect * q) {
  struct rect p = {px, py, pw, ph};
  return collides_2D(&p, q);
}

void main(int argc, char * argv[]) {
  char tmp_buff[256];

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
  
  // world
  struct map_node fountain = {"fountain.tmx"};
  struct map_node forest = {"forest.tmx"};
  struct map_node elf = {"elf.tmx"};
  struct map_node fire = {"fire.tmx"};
  struct map_node dragon = {"dragon.tmx"};
  struct map_node wizard = {"wizard.tmx"};
  struct map_node cave = {"cave.tmx"};
  fountain.west = &elf; elf.east = &fountain;
  fountain.north = &dragon; dragon.south = &fountain;
  dragon.west = &fire; fire.east = &dragon;
  dragon.east = &wizard; wizard.west = &dragon;
  wizard.south = &forest; forest.north = &wizard;
  struct dict warps;
  dict_init(&warps, 0, true, false);
  dict_set(&warps, "cave", &cave);
  dict_set(&warps, "wizard", &wizard);
  struct map_node * map = NULL;
  struct map_node * next_map = &fountain;

  // tileset
  struct dict animated_tiles;
  struct dict blocking_tiles;
  int tileset_columns;
  xmlChar * tileset_image = NULL;
  dict_init(&animated_tiles, sizeof(struct tile_animation), false, false);
  dict_init(&blocking_tiles, 0, false, false);

  // map
  const int TS = 16;
  const int MAP_COL = 16;
  const int MAP_ROW = 11;
  const int HUD_H = 3 * TS;
  const int LAYERS_CAPACITY = 2;
  int layers[LAYERS_CAPACITY][MAP_ROW][MAP_COL];
  int layers_size;
  bool warping = false;
  struct rect warp;
  struct map_node * warp_map;
  struct rect item;
  const char * item_id = NULL;
  struct rect npc;
  char * npc_id = NULL;
  struct dict npcs;
  dict_init(&npcs, 0, true, false);
  dict_set(&npcs, "garden", "This garden belongs to princess purple dress. No trespassing please.");
  dict_set(&npcs, "elf", "I'm hungry. I want candy.");
  dict_set(&npcs, "wizard", "I cannot find my magic staff. Will you help?");
  dict_set(&npcs, "bottle", "The chest is locked.");
  const char * message = NULL;
  struct dict npc_state;
  dict_init(&npc_state, 0, true, true);
  struct dict npc_res;
  dict_init(&npc_res, 0, true, false);
  dict_set(&npc_res, "elf", "boggart.CC0.crawl-tiles.png");
  dict_set(&npc_res, "dragon", "dragon.CC0.crawl-tiles.png");
  dict_set(&npc_res, "wizard", "human.CC0.crawl-tiles.png");
  dict_set(&npc_res, "bottle", "chest_2_closed.CC0.crawl-tiles.png");
  for(int i = 0; i < npc_res.size; i++) dprintf(gfx, "cache %s\n", dict_get_by_index(&npc_res, i));
  dprintf(gfx, "cache chest_2_open.CC0.crawl-tiles.png\n");
  dict_set(&npc_res, "flame", "dngn_altar_makhleb_flame%d.CC0.crawl-tiles.png"); // 1 to 8
  for(int i = 1; i <= 8; i++) dprintf(gfx, "cache dngn_altar_makhleb_flame%d.CC0.crawl-tiles.png\n", i);
  struct dict ignore;
  dict_init(&ignore, 0, true, false);

  // states
  double px = 0;
  double py = 0;
  struct rect forward;
  forward.w = TS;
  forward.h = TS;
  const char * held_item = NULL;

  // game setup
  int W = 256;
  int H = 224;
  //dprintf(snd, "stream bg.ogg\n");
  dprintf(gfx, "title %s\n", argv[0]);
  dprintf(gfx, "hq\n");
  dprintf(gfx, "window %d %d\n", W, H);
  dprintf(gfx, "cache DejaVuSans-Bold.ttf\n");
  dprintf(gfx, "cache princess.png\n");
  struct dict items;
  dict_init(&items, 0, true, false);
  dict_set(&items, "cane", "cane.resized.CC0.7soul1.png");
  dict_set(&items, "key", "key.resized.CC0.7soul1.png");
  dict_set(&items, "bottle", "bottle.resized.CC0.7soul1.png");
  dict_set(&items, "water", "water.resized.CC0.7soul1.png");
  dict_set(&items, "heart", "heart.resized.CC0.7soul1.png");
  dict_set(&items, "staff", "staff02.CC0.crawl-tiles.png");
  dict_set(&items, "spell", "scroll-thunder.CC0.pixel-boy.png");
  for(int i = 0; i < items.size; i++) dprintf(gfx, "cache %s\n", dict_get_by_index(&items, i));

  // game loop
  bool running = true;
  bool focused = true;
  bool loading = true;
  double delta_time = 0;
  double delta_time_worst = 0;
  double step_per_seconds = 300;
  int facing_index = 0;
  bool facing_mirror = false;
  int facing_frame = 0;
  uint64_t walking_t0;
  uint64_t tick = 0;
  const int walking_period = 300;
  struct rect collision = {1, 14, 12, 8}; // hard-coded princess collision box
  while(running) {
    // parse map created with Tiled (https://www.mapeditor.org/)
    // [with assumptions on tile size, single tileset across all maps, single warp rect, single npc)
    if(next_map) {
      layers_size = 0;
      warp_map = NULL;
      item_id = NULL;
      if(npc_id) free(npc_id);
      npc_id = NULL;
      xmlDoc * doc = xmlParseFile(next_map->filename); if(!doc) { printf("xmlParseFile(%s) failed.\n", next_map->filename); exit(EXIT_FAILURE); }
      xmlNode * mcur = xmlDocGetRootElement(doc); if(!mcur) { printf("xmlDocGetRootElement() is null.\n"); exit(EXIT_FAILURE); }
      mcur = mcur->xmlChildrenNode;
      while(mcur != NULL) {
        // load tileset
        if(!tileset_image && xmlStrcmp(mcur->name, "tileset") == 0) {
          xmlChar * source = xmlGetProp(mcur, "source");
          xmlDoc * tileset = xmlParseFile(source); if(!tileset) { printf("xmlParseFile(%s) failed.\n", source); exit(EXIT_FAILURE); }
          xmlNode * tcur = xmlDocGetRootElement(tileset); if(!tcur) { printf("xmlDocGetRootElement() is null.\n"); exit(EXIT_FAILURE); }
          xmlFree(source);
          xmlChar * str_columns = xmlGetProp(tcur, "columns");
          tileset_columns = strtol(str_columns, NULL, 10);
          xmlFree(str_columns);
          tcur = tcur->xmlChildrenNode;
          while(tcur != NULL) {
            if(xmlStrcmp(tcur->name, "image") == 0) {
              tileset_image = xmlGetProp(tcur, "source");
              dprintf(gfx, "cache %s\n", tileset_image);
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
              if(layers_size == LAYERS_CAPACITY) { printf("layers array full\n"); exit(EXIT_FAILURE); }
              int index = layers_size++;
              xmlChar * data = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
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
                    if(row >= MAP_ROW) { printf("too many row in map data\n"); exit(EXIT_FAILURE); }
                    if(col >= MAP_COL) { printf("too many col in map data\n"); exit(EXIT_FAILURE); }
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
        // object
        else if(xmlStrcmp(mcur->name, "objectgroup") == 0) {
          xmlNode * node = mcur->xmlChildrenNode;
          while(node != NULL) {
            if(xmlStrcmp(node->name, "object") == 0) {
              xmlChar * type = xmlGetProp(node, "type");
              if(type) {
                if(xmlStrcmp(type, "spawn") == 0) {
                  xmlChar * x = xmlGetProp(node, "x");
                  xmlChar * y = xmlGetProp(node, "y");
                  if(warping || !map) {
                    px = strtod(x, NULL) - collision.w/2 - collision.x;
                    py = strtod(y, NULL) - collision.h/2 - collision.y;
                  }
                  xmlFree(y);
                  xmlFree(x);
                }
                else if(xmlStrcmp(type, "warp") == 0) {
                  xmlChar * x = xmlGetProp(node, "x");
                  xmlChar * y = xmlGetProp(node, "y");
                  xmlChar * w = xmlGetProp(node, "width");
                  xmlChar * h = xmlGetProp(node, "height");
                  xmlChar * name = xmlGetProp(node, "name");
                  warp.x = strtod(x, NULL);
                  warp.w = strtod(w, NULL);
                  warp.y = strtod(y, NULL);
                  warp.h = strtod(h, NULL);
                  warp_map = dict_get(&warps, name);
                  if(!warp_map) { printf("invalid warp name %s\n", name); exit(EXIT_FAILURE); }
                  xmlFree(name);
                  xmlFree(h);
                  xmlFree(w);
                  xmlFree(y);
                  xmlFree(x);
                }
                else if(xmlStrcmp(type, "item") == 0) {
                  xmlChar * x = xmlGetProp(node, "x");
                  xmlChar * y = xmlGetProp(node, "y");
                  xmlChar * w = xmlGetProp(node, "width");
                  xmlChar * h = xmlGetProp(node, "height");
                  xmlChar * name = xmlGetProp(node, "name");
                  item_id = dict_get(&items, name);
                  if(dict_has(&ignore, item_id)) item_id = NULL;
                  item.x = strtod(x, NULL);
                  if(w) {
                    item.w = strtod(w, NULL);
                  } else {
                    item.w = TS;
                    item.x -= TS / 2;
                  }
                  item.y = strtod(y, NULL);
                  if(h) {
                    item.h = strtod(h, NULL);
                  } else {
                    item.h = TS;
                    item.y -= TS / 2;
                  }
                  xmlFree(name);
                  xmlFree(h);
                  xmlFree(w);
                  xmlFree(y);
                  xmlFree(x);
                } else if(xmlStrcmp(type, "npc") == 0) {
                  xmlChar * x = xmlGetProp(node, "x");
                  xmlChar * y = xmlGetProp(node, "y");
                  xmlChar * w = xmlGetProp(node, "width");
                  xmlChar * h = xmlGetProp(node, "height");
                  xmlChar * name = xmlGetProp(node, "name");
                  printf("NPC in map is %s\n", name);
                  npc.x = strtod(x, NULL);
                  if(w) {
                    npc.w = strtod(w, NULL);
                  } else {
                    npc.w = TS;
                    npc.x -= TS / 2;
                  }
                  npc.y = strtod(y, NULL);
                  if(h) {
                    npc.h = strtod(h, NULL);
                  } else {
                    npc.h = TS;
                    npc.y -= TS / 2;
                  }
                  if(!dict_has(&ignore, name)) {
                    npc_id = strdup(name);
                    printf("NPC stored\n");
                  } else {
                    printf("NPC ignored\n");
                  }
                  xmlFree(name);
                  xmlFree(h);
                  xmlFree(w);
                  xmlFree(y);
                  xmlFree(x);
                }
              }
              xmlFree(type);
            }
            node = node->next;
          }
        }
        mcur = mcur->next;
      }
      xmlFreeDoc(doc);
      if(!tileset_image) { printf("did not find anything tileset image while parsing map\n"); exit(EXIT_FAILURE); }
      map = next_map;
      next_map = NULL;
      warping = false;
    }

    // input
    sprintf(emm->msg, focused? "" : "no-focus-mode"); error = srr_send(&evt, strlen(emm->msg)); if(error) { printf("srr_send(evt): %s\n", error); exit(EXIT_FAILURE); }
    running &= !evt_released(&evt, K_ESC);
    if(!loading) {
      // walking
      struct evt_axis_and_triggers_normalized axis = evt_deadzoned(evt_axis_and_triggers(&evt, 0), .2, .2);
      if(evt_held(&evt, G0_DOWN) || evt_held(&evt, K_S)) axis.ly = fmin(1, axis.ly + 1);
      if(evt_held(&evt, G0_UP) || evt_held(&evt, K_W)) axis.ly = fmax(-1, axis.ly - 1);
      if(evt_held(&evt, G0_RIGHT) || evt_held(&evt, K_D)) axis.lx = fmin(1, axis.lx + 1);
      if(evt_held(&evt, G0_LEFT) || evt_held(&evt, K_A)) axis.lx = fmax(-1, axis.lx - 1);
      if(axis.lx != 0 || axis.ly != 0) {
        // up/down
        if(fabs(axis.ly) > fabs(axis.lx)) {
          facing_index = (axis.ly < 0)? 2 : 0;
          // double up number of animation frame by mirroring half the time
          facing_mirror = (tick - walking_t0) % (walking_period * 2) < walking_period;
          forward.y = py + collision.y + ((axis.ly < 0)? -forward.h : collision.h);
          forward.x = px + collision.x - (forward.w - collision.w) / 2;
        }
        // left/right
        else {
          facing_index = 1;
          facing_mirror = axis.lx < 0; // left is right mirrored
          forward.y = py + collision.y - (forward.h - collision.h) / 2;
          forward.x = px + collision.x + ((axis.lx < 0)? -forward.w: collision.w);
        }
        // two-frame animation
        facing_frame = ((tick - walking_t0) % walking_period < walking_period/2)? 1 : 0;
      } else {
        facing_frame = 0;
        walking_t0 = tick;
      }
      // collision
      {
        // tentative new position
        double nx = px + delta_time * step_per_seconds * axis.lx;
        double ny = py + delta_time * step_per_seconds * axis.ly;
        // test dimensions separately to allow sliding
        // simply test the corners, and assume speed is low so I don't need collision response
        bool blocked_x = false;
        bool break_x = false;
        bool test_npc = npc_id && dict_get(&npc_res, npc_id);
        if(warp_map && collides_2D_dx(nx + collision.x, py + collision.y, collision.w, collision.h, &warp)) { break_x = true; next_map = warp_map; warping = true; }
        if(!break_x && item_id) blocked_x = collides_2D_dx(nx + collision.x, py + collision.y, collision.w, collision.h, &item);
        if(test_npc && !break_x && !blocked_x) blocked_x = collides_2D_dx(nx + collision.x, py + collision.y, collision.w, collision.h, &npc);
        for(int i = 0, x = nx + collision.x; !break_x && !blocked_x && i < 2; i++, x += collision.w) {
          for(int j = 0, y = py + collision.y; !break_x && !blocked_x && j < 2; j++, y += collision.h) {
            if(x < 0 && map->west) { break_x = true; next_map = map->west; nx += MAP_COL * TS - collision.w; }
            else if(x >= MAP_COL * TS && map->east) { break_x = true; next_map = map->east; nx -= MAP_COL * TS - collision.w; }
            else {
              blocked_x |= y < 0 || y >= MAP_ROW * TS || x < 0 || x >= MAP_COL * TS;
              int col = (int)(x / TS);
              int row = (int)(y / TS);
              for(int k = 0; !blocked_x && k < layers_size; k++) {
                int tile = layers[k][row][col] - 1;
                blocked_x |= dict_get(&blocking_tiles, tile);
              }
            }
          }
        }
        bool blocked_y = false;
        bool break_y = false;
        if(warp_map && collides_2D_dx(px + collision.x, ny + collision.y, collision.w, collision.h, &warp)) { break_y = true; next_map = warp_map; warping = true; }
        if(!break_y && item_id) blocked_y = collides_2D_dx(px + collision.x, ny + collision.y, collision.w, collision.h, &item);
        if(test_npc && !break_y && !blocked_y) blocked_y = collides_2D_dx(px + collision.x, ny + collision.y, collision.w, collision.h, &npc);
        for(int i = 0, x = px + collision.x; !break_y && !blocked_y && i < 2; i++, x += collision.w) {
          for(int j = 0, y = ny + collision.y; !break_y && !blocked_y && j < 2; j++, y += collision.h) {
            if(y < 0 && map->north) { break_y = true; next_map = map->north; ny += MAP_ROW * TS - collision.h; }
            else if(y >= MAP_ROW * TS && map->south) { break_y = true; next_map = map->south; ny -= MAP_ROW * TS - collision.h; }
            else {
              blocked_y |= y < 0 || y >= MAP_ROW * TS || x < 0 || x >= MAP_COL * TS;
              int col = (int)(x / TS);
              int row = (int)(y / TS);
              for(int k = 0; !blocked_y && k < layers_size; k++) {
                int tile = layers[k][row][col] - 1;
                blocked_y |= dict_get(&blocking_tiles, tile);
              }
            }
          }
        }
        if(!blocked_x) px = nx;
        if(!blocked_y) py = ny;
      }
      // action button (activate stuff forward, dismiss message box)
      if(evt_released(&evt, G0_EAST) || evt_released(&evt, G0_SOUTH)) {
        // dismiss dialog
        if(message) {
          message = NULL;
        }
        // pickup items
        else if(item_id && collides_2D(&forward, &item)) {
          held_item = item_id;
          item_id = NULL;
          dict_set(&ignore, held_item, true);
        }
        // npc interaction
        else if(npc_id && collides_2D(&forward, &npc)) {
          printf("npc %s\n", npc_id);

          // pre
          int state = dict_get(&npc_state, npc_id);
          if(strcmp(npc_id, "elf") == 0) {
            if(state > 0 && held_item && strcmp(held_item, dict_get(&items, "cane")) == 0) {
              dict_set(&npcs, "elf", "A candy cane! Thank you so much. You may pass.");
              dict_set(&npc_state, npc_id, 2);
              held_item = NULL;
            }
          } else if(strcmp(npc_id, "bottle") == 0) {
            if(state == 0 && held_item && strcmp(held_item, dict_get(&items, "key")) == 0) {
              dict_set(&npcs, "bottle", "You open the chest with the key, and find an empty bottle.");
              dict_set(&npc_state, npc_id, 1);
              held_item = dict_get(&items, "bottle");
              dict_set(&npc_res, "bottle", "chest_2_open.CC0.crawl-tiles.png");
            }
          } else if(strcmp(npc_id, "flame") == 0) {
            if(state == 0 && held_item && strcmp(held_item, dict_get(&items, "water")) == 0) {
              dict_set(&npcs, npc_id, "You douse the flame with your water bottle, and find a magic staff.");
              dict_set(&npc_state, npc_id, 1);
              held_item = dict_get(&items, "staff");
            }
          } else if(strcmp(npc_id, "wizard") == 0) {
            if(state == 1 && held_item && strcmp(held_item, dict_get(&items, "staff")) == 0) {
              dict_set(&npcs, npc_id, "You found my staff. Thank you. Let me teach you the magic spell 'Kaboom'.");
              dict_set(&npc_state, npc_id, 2);
              held_item = dict_get(&items, "spell");
            }
          }

          // mid
          message = dict_get(&npcs, npc_id);
          if(message) printf("%s\n", message);
          
          // post
          state = dict_get(&npc_state, npc_id);
          if(strcmp(npc_id, "elf") == 0) {
            if(state == 0) {
              dict_set(&npcs, "elf", "I'm so hungry. I really want candy!");
              dict_set(&npc_state, npc_id, 1);
            } else if(state == 2) {
              dict_set(&ignore, "elf", true);
              free(npc_id); npc_id = NULL;
            }
          } else if(strcmp(npc_id, "bottle") == 0) {
            if(state == 1) {
              dict_set(&npcs, "bottle", "The chest is empty.");
              dict_set(&npc_state, npc_id, 2);
            }
          } else if(strcmp(npc_id, "flame") == 0) {
            if(state == 1) {
              dict_set(&ignore, "flame", true);
              free(npc_id); npc_id = NULL;
            }
          } else if(strcmp(npc_id, "wizard") == 0) {
            if(state == 0) {
              printf("moving to state 1\n");
              dict_set(&npc_state, npc_id, 1);
            } else if(state == 2) {
              dict_set(&npcs, npc_id, "Thank you for returning my staff.");
            }
          }
        }
      }

      // hud
      dprintf(gfx, "fill 000000 0 0 %d %d\n", W, HUD_H);
      // draw tilemap
      for(int i = 0; i < layers_size; i++) {
        for(int row = 0; row < MAP_ROW; row++) {
          for(int col = 0; col < MAP_COL; col++) {
            int tile = layers[i][row][col];
            if(tile != 0) {
              tile = tile - 1;
              // handle animated tiles
              struct tile_animation * anim = dict_get(&animated_tiles, tile);
              if(anim) {
                uint64_t t = tick % anim->total_duration;
                for(int i = 0; i < anim->size; i++) {
                  if(t < anim->durations[i]) { tile = anim->ids[i]; break; }
                  t-= anim->durations[i];
                }
              }
              int x = TS * col;
              int y = TS * row + HUD_H;
              int tx = TS * (tile % tileset_columns);
              int ty = TS * (tile / tileset_columns);
              dprintf(gfx, "draw %s %d %d %d %d %d %d\n", tileset_image, tx, ty, TS, TS, x, y);
            }
          }
        }
      }
      // draw item
      if(item_id && strcmp(item_id, dict_get(&items, "water")) != 0) {
        dprintf(gfx, "draw %s %f %f\n", item_id, item.x, item.y + HUD_H);
      }
      if(held_item) {
        dprintf(gfx, "draw %s %f %f\n", held_item, (W - TS) / 2.0, HUD_H / 2.0 - TS);
      }
      // draw npc
      if(npc_id) {
        const char * res = dict_get(&npc_res, npc_id);
        if(res) {
          // case flame animation
          if(strcmp(npc_id, "flame") == 0) {
            uint64_t flame_period = 400;
            snprintf(tmp_buff, 256, res, (int)((tick % flame_period) / (double)flame_period * 8 + 1));
            res = tmp_buff;
          }
          double w = npc.w;
          double h = npc.h;
          double x = npc.x;
          double y = npc.y;
          // case dragon dimensions are his patrol region, not draw size, and neither is drawn position
          if(strcmp(npc_id, "dragon") == 0) {
            w = h = 2 * TS;
            x = fmin(fmax(npc.x, px), npc.x + npc.w - w);
            y = npc.y + npc.h - h;
          }
          dprintf(gfx, "draw %s %f %f %f %f\n", res, x, y + HUD_H, w, h);
        }
      }
      // draw player
      dprintf(gfx, "draw princess.png %d %d 14 24 %f %f %s\n", facing_frame * 14, facing_index * 24, px, py + HUD_H, facing_mirror? "mx" : "");

      // message box
      if(message) {
        double w = W * .8;
        double h = (H - HUD_H) * .3;
        int n = h / 10;
        double x = (W - w) / 2;
        double y = (H - HUD_H - h) / 2 + HUD_H;
        dprintf(gfx, "fill 88888888 %f %f %f %f\n", x, y, w, h);
        dprintf(gfx, "text DejaVuSans-Bold.ttf %f %f %f %f center left %d noclip 0 ffffff 000000 1 %s\n", x, y, w, h, n, message);
      }

      // fps
      dprintf(gfx, "text DejaVuSans-Bold.ttf 1 0 255 16 top left 2 noclip 0 ffffff 00ff00 0 ms:%d\\nfps:%2.1f\n", (int)(delta_time * 1000), 1 / (double)delta_time);
    }
    // flush
    dprintf(gfx, "flush\n");
    sprintf(gmm->msg, "flush delta stat %s", tileset_image); error = srr_send(&gfs, strlen(gmm->msg)); if(error) { printf("srr_send(gfs): %s\n", error); exit(EXIT_FAILURE); }
    focused = gmm->msg[0];
    running &= !gmm->msg[1];
    int i = 10;
    if(gmm->msg[i++] != GFX_STAT_DLT) { printf("unexpected stat result, wanted delta time\n"); exit(EXIT_FAILURE); }
    tick += *(int *)&gmm->msg[i];
    delta_time = *(int *)&gmm->msg[i] / 1000.0;
    if(tick > 1000 && delta_time > delta_time_worst) delta_time_worst = delta_time;
    i+= 4;
    //printf("%d\t%d\n", (int)(delta_time_worst * 1000), (int)(delta_time * 1000));
    if(gmm->msg[i] == GFX_STAT_ERR) { printf("stat error %c%c%c\n", gmm->msg[i+1], gmm->msg[i+2], gmm->msg[i+3]); exit(EXIT_FAILURE); }
    if(gmm->msg[i++] != GFX_STAT_IMG) { printf("unexpected stat result, wanted img\n"); exit(EXIT_FAILURE); }
    int w = *(int *)&gmm->msg[i]; i+= 4;
    int h = *(int *)&gmm->msg[i]; i+= 4;
    loading = w == 0;
    if(loading) { printf("loading progress %f\n", h / 1000.0); }
  }

  // disconnect
  if(npc_id) free(npc_id);
  dict_free(&blocking_tiles);
  dict_free(&animated_tiles);
  xmlFree(tileset_image);
  close(snd);
  close(gfx);
  error = srr_disconnect(&gfs); if(error) { printf("srr_disconnect(gfx): %s\n", error); exit(EXIT_FAILURE); }
  error = srr_disconnect(&evt); if(error) { printf("srr_disconnect(evt): %s\n", error); exit(EXIT_FAILURE); }
}
