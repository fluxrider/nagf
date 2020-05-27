// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o example_server example_server.c -L. -l:msgmgr.so -l:msglib.so
// LD_LIBRARY_PATH=. ./example_server
#include "msgmgr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void main(void) {
  // connect
  const char * error;
  struct msgmgr server;
  error = msgmgr_init(&server, "/example-msgmgr", 8192, true, false, 3); if(error) { printf("msgmgr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct msgmgr_direct * mem = msgmgr_direct(&server);

  // wait for a message from the client
  error = msgmgr_receive(&server); if(error) { printf("msgmgr_receive: %s\n", error); exit(EXIT_FAILURE); }
  printf("length: %u\n", mem->length);
  printf("msg: %s\n", mem->msg);
  strcpy(mem->msg, "whatever");
  error = msgmgr_reply(&server, strlen(mem->msg)); if(error) { printf("msgmgr_reply: %s\n", error); exit(EXIT_FAILURE); }
  // disconnect
  sleep(1); // give client some time before destroying channel
  error = msgmgr_disconnect(&server); if(error) { printf("msgmgr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
}
