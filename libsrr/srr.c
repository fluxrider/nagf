// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -fPIC -shared -o libsrr.so srr.c backend_shm.c -lrt -pthread

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "backend_shm.h"
#include "srr.h"

#define CAP 512

const char * srr_init(struct srr * self, const char * name, size_t length, bool is_server, bool use_multi_client_lock, double timeout) {
  self->closed = true;
  self->length = length;
  self->is_server = is_server;
  self->use_multi_client_lock = use_multi_client_lock;
  self->timeout = timeout;
  int error;
  int line;
  self->shm = srr_shm_connect(name, length, is_server, &error, &line);
  if(error != 0) { snprintf(self->error_msg, CAP, "srr_shm_connect:%d: %s", line, strerror(error)); return self->error_msg; }
  self->msg = srr_shm_get_mem(self->shm);
  self->closed = false;
  return NULL;
}

const char * srr_disconnect(struct srr * self) {
  if(self->closed) return NULL;
  int line;
  const char * error = srr_shm_disconnect(self->shm, &line);
  self->closed = true;
  if(error != 0) { snprintf(self->error_msg, CAP, "srr_shm_disconnect:%d: %s", line, error); return self->error_msg; }
  return NULL;
}

// NOTE: reply is assumed to be a buffer of size self->length TODO actually, minus sizeof semaphores and locks!
const char * srr_send_dx(struct srr * self, uint8_t * data, size_t length, uint8_t * reply, uint32_t * reply_length) {
  if(self->is_server) return "srr_send is for clients, not servers";
  bool direct_write = data == &self->msg[4];
  bool direct_read = reply == &self->msg[4];
  bool direct_length = reply_length == (uint32_t *)self->msg;
  if(self->use_multi_client_lock && (direct_write || direct_read || direct_length)) return "use_multi_client_lock is enabled, yet you touch the shared memory directly";
  int line;
  const char * error;
  // lock
  if(self->use_multi_client_lock) {
    error = srr_shm_lock(self->shm, &line);
    if(error) { snprintf(self->error_msg, CAP, "srr_shm_lock:%d: %s", line, error); return self->error_msg; }
  }
  // header: 32-bit length of data
  *(uint32_t *)self->msg = (uint32_t) length;
  // body: copy the data
  if(!direct_write) memcpy(&self->msg[4], data, length);
  // send
  error = srr_shm_post(self->shm, &line);
  if(error) { snprintf(self->error_msg, CAP, "srr_shm_post:%d: %s", line, error); return self->error_msg; }
  // wait for the reply
  error = srr_shm_wait(self->shm, self->timeout, &line);
  if(error) { snprintf(self->error_msg, CAP, "srr_shm_post:%d: %s", line, error); return self->error_msg; }
  // read header
  if(!direct_length) *reply_length = *(uint32_t *)self->msg;
  if(!direct_read) memcpy(reply, &self->msg[4], *reply_length);
  // unlock
  if(self->use_multi_client_lock) {
    error = srr_shm_unlock(self->shm, &line);
    if(error) { snprintf(self->error_msg, CAP, "srr_shm_unlock:%d: %s", line, error); return self->error_msg; }
  }
  return NULL;
}

const char * srr_send(struct srr * self, size_t length) {
  if(self->use_multi_client_lock) return "when use_multi_client_lock is enabled, you must use srr_send_dx";
  return srr_send_dx(self, &self->msg[4], length, &self->msg[4], (uint32_t *)self->msg);
}

struct srr_direct * srr_direct(struct srr * self) {
  return (struct srr_direct *)self->msg;
}

const char * srr_receive(struct srr * self) {
  if(!self->is_server) return "srr_receive is for servers, not clients";
  int line;
  const char * error = srr_shm_wait(self->shm, self->timeout, &line);
  if(error) { snprintf(self->error_msg, CAP, "srr_shm_wait:%d: %s", line, error); return self->error_msg; }
  // the server can now use the srr_direct struct to read the message, and write the reply
  return NULL;
}

const char * srr_reply(struct srr * self, size_t length) {
  if(!self->is_server) return "srr_reply is for servers, not clients";
  *(uint32_t *)self->msg = (uint32_t) length;
  int line;
  const char * error = srr_shm_post(self->shm, &line);
  if(error) { snprintf(self->error_msg, CAP, "srr_shm_post:%d: %s", line, error); return self->error_msg; }
  return NULL;
}
