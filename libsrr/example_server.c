// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o example_server example_server.c -L. -lsrr
// LD_LIBRARY_PATH=. ./example_server
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "srr.h"

void main(void) {
  // connect
  const char * error;
  struct srr server;
  error = srr_init(&server, "/example-srr", 8192, true, false, 3); if(error) { printf("srr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct srr_direct * mem = srr_direct(&server);

  // wait for a message from the client
  error = srr_receive(&server); if(error) { printf("srr_receive: %s\n", error); exit(EXIT_FAILURE); }
  printf("length: %u\n", mem->length);
  printf("msg: %s\n", mem->msg);
  strcpy(mem->msg, "whatever");
  error = srr_reply(&server, strlen(mem->msg)); if(error) { printf("srr_reply: %s\n", error); exit(EXIT_FAILURE); }
  // disconnect
  sleep(1); // give client some time before destroying channel
  error = srr_disconnect(&server); if(error) { printf("srr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
}
