// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o example_client example_client.c -L. -lsrr
// LD_LIBRARY_PATH=. ./example_client
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "srr.h"

void main(void) {
  // connect
  const char * error;
  struct srr client;
  error = srr_init(&client, "/example-srr", 8192, false, false, 3); if(error) { printf("srr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct srr_direct * mem = srr_direct(&client);

  // send message to server and print reply
  strcpy(mem->msg, "hello");
  error = srr_send(&client, strlen(mem->msg)); if(error) { printf("srr_send: %s\n", error); exit(EXIT_FAILURE); }
  printf("length: %u\n", mem->length);
  printf("msg: %s\n", mem->msg);
  
  // disconnect
  error = srr_disconnect(&client); if(error) { printf("srr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
}
