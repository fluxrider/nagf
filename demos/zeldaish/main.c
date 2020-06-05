// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o zeldaish main.c ../../utils/evt-util.c -L../../libsrr -I../../libsrr -I../../utils -lsrr $(pkg-config --libs --cflags libxml-2.0)
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
#include "srr.h"
#include "evt-util.h"

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
  xmlChar * tileset_image = NULL;
  int tilewidth;
  int tileheight;
  const int layers_capacity = 10;
  xmlChar * layers[layers_capacity];
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
      tilewidth = strtol(str_tilewidth, NULL, 10);
      tileheight = strtol(str_tileheight, NULL, 10);
      xmlFree(str_tilewidth);
      xmlFree(str_tileheight);
      tcur = tcur->xmlChildrenNode;
      while(tcur != NULL) {
        if(xmlStrcmp(tcur->name, "image") == 0) {
          tileset_image = xmlGetProp(tcur, "source");
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
          layers[layers_size++] = xmlNodeListGetString(map, node->xmlChildrenNode, 1);
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
  dprintf(gfx, "cache %s\n", tileset_image);

  // game loop
  bool running = true;
  bool focused = true;
  while(running) {
    // input
    sprintf(emm->msg, focused? "" : "no-focus-mode"); error = srr_send(&evt, strlen(emm->msg)); if(error) { printf("srr_send(evt): %s\n", error); exit(EXIT_FAILURE); }

    // gfx
    dprintf(gfx, "flush\n");
    sprintf(gmm->msg, "flush"); error = srr_send(&gfs, strlen(gmm->msg)); if(error) { printf("srr_send(gfs): %s\n", error); exit(EXIT_FAILURE); }
    focused = gmm->msg[0];
    running = !gmm->msg[1];
  }

  // disconnect
  xmlFree(tileset_image);
  for(int i = 0; i < layers_size; i++) { xmlFree(layers[i]); }
  close(snd);
  close(gfx);
  error = srr_disconnect(&gfs); if(error) { printf("srr_disconnect(gfx): %s\n", error); exit(EXIT_FAILURE); }
  error = srr_disconnect(&evt); if(error) { printf("srr_disconnect(evt): %s\n", error); exit(EXIT_FAILURE); }
}
