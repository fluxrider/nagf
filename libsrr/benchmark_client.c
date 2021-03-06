// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o benchmark_client benchmark_client.c -L. -lsrr
// LD_LIBRARY_PATH=. ./benchmark_client
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include "srr.h"

uint64_t currentTimeMillis() {
  struct timespec tp;
  if(clock_gettime(CLOCK_MONOTONIC, &tp) == -1) { perror("read"); exit(1); }
  return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

void main(int argc, char * argv[]) {
  srand(time(NULL));
  int seconds = atoi(argv[1]);
  bool multi = argc >= 3;
  bool bigmsg = argc >= 4;

  // connect
  const char * error;
  struct srr client;
  error = srr_init(&client, "/benchmark-srr", 8192, false, multi, 2); if(error) { printf("srr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct srr_direct * mem = multi? malloc(sizeof(struct srr_direct) + sizeof(uint32_t)) : srr_direct(&client);

  // send random ints for N seconds
  uint64_t t0 = currentTimeMillis();
  uint64_t count = 0;
  do {
    uint32_t x = rand();
    *(uint32_t *)mem->msg = x;
    error = srr_send_dx(&client, mem->msg, bigmsg? 8192 : sizeof(uint32_t), mem->msg, &mem->length);
    if(error) { printf("srr_send_dx: %s\n", error); exit(EXIT_FAILURE); }
    if(mem->length != sizeof(uint32_t)) { printf("unexpected message size: %u\n", mem->length); exit(EXIT_FAILURE); }
    // verify we got (x+5)
    if(*(uint32_t*)mem->msg != x + 5) { printf("bad answer: %u != %u\n", *(uint32_t*)mem->msg, x + 5); exit(EXIT_FAILURE); }
    count++;
  } while(currentTimeMillis() < t0 + seconds * 1000);
  printf("%f\n", count / (double)seconds);
  
  // disconnect
  error = srr_disconnect(&client); if(error) { printf("srr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
}
