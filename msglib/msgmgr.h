#pragma once
// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct msgmgr_direct {
  uint32_t length;
  uint8_t msg[];
};

struct msgmgr {
  size_t length;
  bool is_server;
  bool use_multi_client_lock;
  double timeout;
  void * msgmgr;
  uint8_t * msg;
  char error_msg[512];
};

const char * msgmgr_init(struct msgmgr * self, const char * name, size_t length, bool is_server, bool use_multi_client_lock, double timeout);
const char * msgmgr_disconnect(struct msgmgr * self);
const char * msgmgr_send_dx(struct msgmgr * self, uint8_t * data, size_t length, uint8_t * reply, uint32_t * reply_length);
const char * msgmgr_send(struct msgmgr * self, size_t length);
struct msgmgr_direct * msgmgr_direct(struct msgmgr * self);
const char * msgmgr_receive(struct msgmgr * self);
const char * msgmgr_reply(struct msgmgr * self, size_t length);
