#pragma once
// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct srr_direct {
  uint32_t length;
  uint8_t msg[];
};

struct srr {
  size_t length;
  bool is_server;
  bool use_multi_client_lock;
  double timeout;
  void * shm;
  uint8_t * msg;
  char error_msg[512];
  bool closed;
};

const char * srr_init(struct srr * self, const char * name, size_t length, bool is_server, bool use_multi_client_lock, double timeout);
const char * srr_disconnect(struct srr * self);
const char * srr_send_dx(struct srr * self, uint8_t * data, size_t length, uint8_t * reply, uint32_t * reply_length);
const char * srr_send(struct srr * self, size_t length);
struct srr_direct * srr_direct(struct srr * self);
const char * srr_receive(struct srr * self);
const char * srr_reply(struct srr * self, size_t length);
