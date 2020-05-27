// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o benchmark_server benchmark_server.c -L. -lsrr
// LD_LIBRARY_PATH=. ./benchmark_server
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "srr.h"

void main(void) {
  // connect
  const char * error;
  struct srr server;
  error = srr_init(&server, "/benchmark-srr", 8192, true, false, 2); if(error) { printf("srr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct srr_direct * mem = srr_direct(&server);

  // receive loop
  while(1) {
    error = srr_receive(&server); if(error) { printf("srr_receive: %s\n", error); exit(EXIT_FAILURE); }
    if(mem->length < sizeof(uint32_t)) printf("unexpected message size: %u\n", mem->length);
    else {
      // return (x+5)
      *(uint32_t*)mem->msg += 5; 
    }
    error = srr_reply(&server, sizeof(uint32_t)); if(error) { printf("srr_reply: %s\n", error); exit(EXIT_FAILURE); }
  }
}
