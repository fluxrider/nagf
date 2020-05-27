// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o benchmark_client benchmark_client.c -L. -lsrr
// LD_LIBRARY_PATH=. ./benchmark_server
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include "srr.h"

uint64_t currentTimeMillis() {
  struct timespec tp;
  if(clock_gettime(CLOCK_MONOTONIC, &tp) == -1) { perror("read"); exit(1); }
  return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

void main(void) {
  srand(time(NULL));
  
  // connect
  const char * error;
  struct srr client;
  error = srr_init(&client, "/benchmark-srr-c", 8192, false, false, 2); if(error) { printf("srr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct srr_direct * mem = srr_direct(&client);

  // send random ints for 2 seconds
  uint64_t t0 = currentTimeMillis();
  uint64_t count = 0;
  do {
    uint32_t x = rand();
    *(uint32_t *)mem->msg = x;
    error = srr_send(&client, sizeof(uint32_t)); if(error) { printf("srr_send: %s\n", error); exit(EXIT_FAILURE); }
    if(mem->length != sizeof(uint32_t)) { printf("unexpected message size: %u\n", mem->length); exit(EXIT_FAILURE); }
    else {
      // verify we got (x+5)
      if(*(uint32_t*)mem->msg != x + 5) { printf("bad answer: %u != %u\n", *(uint32_t*)mem->msg, x + 5); exit(EXIT_FAILURE); }
      count++;
    }
  } while(currentTimeMillis() < t0 + 5000);
  printf("%" PRIu64 " send/receive/reply in 5 seconds (%f per second)\n", count, count / 5.0);
  
  // disconnect
  error = srr_disconnect(&client); if(error) { printf("srr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
}
