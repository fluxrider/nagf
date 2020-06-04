// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o zeldaish main.c ../../utils/evt-util.c -L../../libsrr -I../../libsrr -I../../utils -lsrr
// LD_LIBRARY_PATH=../../libsrr ./zeldaish
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
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

  // tmp
  dprintf(snd, "stream bg1.ogg\n");
  dprintf(gfx, "title %s\n", argv[0]);

  // game loop
  bool running = true;
  while(running) {
    // input
    error = srr_send(&evt, 0); if(error) { printf("srr_send(evt): %s\n", error); exit(EXIT_FAILURE); }

    // gfx
    dprintf(gfx, "flush\n");
    sprintf(gmm->msg, "flush");
    error = srr_send(&gfs, strlen(gmm->msg)); if(error) { printf("srr_send(gfs): %s\n", error); exit(EXIT_FAILURE); }
  }

  // disconnect
  close(gfx);
  error = srr_disconnect(&gfs); if(error) { printf("srr_disconnect(gfx): %s\n", error); exit(EXIT_FAILURE); }
  error = srr_disconnect(&evt); if(error) { printf("srr_disconnect(evt): %s\n", error); exit(EXIT_FAILURE); }
}
