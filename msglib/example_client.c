// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o example_client example_client.c -L. -l:msgmgr.so -l:msglib.so
// LD_LIBRARY_PATH=. ./example_client
#include "msgmgr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void main(void) {
  // connect
  const char * error;
  struct msgmgr client;
  error = msgmgr_init(&client, "/example-msgmgr", 8192, false, false, 3); if(error) { printf("msgmgr_init: %s\n", error); exit(EXIT_FAILURE); }
  struct msgmgr_direct * mem = msgmgr_direct(&client);

  // send message to server and print reply
  strcpy(mem->msg, "hello");
  error = msgmgr_send(&client, strlen(mem->msg)); if(error) { printf("msgmgr_send: %s\n", error); exit(EXIT_FAILURE); }
  printf("length: %u\n", mem->length);
  printf("msg: %s\n", mem->msg);
  
  // disconnect
  error = msgmgr_disconnect(&client); if(error) { printf("msgmgr_disconnect: %s\n", error); exit(EXIT_FAILURE); }
}
