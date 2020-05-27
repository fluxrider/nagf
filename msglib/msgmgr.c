// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -fPIC -shared -o msgmgr.so msgmgr.c

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "msglib.h"
#include "msgmgr.h"

#define CAP 512

const char * msgmgr_init(struct msgmgr * self, const char * name, size_t length, bool is_server, bool use_multi_client_lock, double timeout) {
  self->length = length;
  self->is_server = is_server;
  self->use_multi_client_lock = use_multi_client_lock;
  self->timeout = timeout;
  int error;
  int line;
  self->msgmgr = msglib_connect(name, length, is_server, &error, &line);
  if(error != 0) { snprintf(self->error_msg, CAP, "msglib_connect:%d: %s", line, strerror(error)); return self->error_msg; }
  self->msg = msglib_get_mem(self->msgmgr);
  return NULL;
}

const char * msgmgr_disconnect(struct msgmgr * self) {
  int line;
  const char * error = msglib_disconnect(self->msgmgr, &line);
  if(error != 0) { snprintf(self->error_msg, CAP, "msglib_disconnect:%d: %s", line, error); return self->error_msg; }
  return NULL;
}

// NOTE: reply is assumed to be a buffer of size self->length TODO actually, minus sizeof semaphores and locks!
const char * msgmgr_send_dx(struct msgmgr * self, uint8_t * data, size_t length, uint8_t * reply, uint32_t * reply_length) {
  if(self->is_server) return "msgmgr_send is for clients, not servers";
  bool direct_write = data == &self->msg[4];
  bool direct_read = reply == &self->msg[4];
  bool direct_length = reply_length == (uint32_t *)self->msg;
  if(self->use_multi_client_lock && (direct_write || direct_read || direct_length)) return "use_multi_client_lock is enabled, yet you touch the shared memory directly";
  int line;
  const char * error;
  // lock
  if(self->use_multi_client_lock) {
    error = msglib_lock(self->msgmgr, &line);
    if(error) { snprintf(self->error_msg, CAP, "msglib_lock:%d: %s", line, error); return self->error_msg; }
  }
  // header: 32-bit length of data
  *(uint32_t *)self->msg = (uint32_t) length;
  // body: copy the data
  if(!direct_write) memcpy(&self->msg[4], data, length);
  // send
  error = msglib_post(self->msgmgr, &line);
  if(error) { snprintf(self->error_msg, CAP, "msglib_post:%d: %s", line, error); return self->error_msg; }
  // wait for the reply
  error = msglib_wait(self->msgmgr, self->timeout, &line);
  if(error) { snprintf(self->error_msg, CAP, "msglib_post:%d: %s", line, error); return self->error_msg; }
  // read header
  if(!direct_length) *reply_length = *(uint32_t *)self->msg;
  if(!direct_read) memcpy(reply, &self->msg[4], *reply_length);
  // unlock
  if(self->use_multi_client_lock) {
    error = msglib_unlock(self->msgmgr, &line);
    if(error) { snprintf(self->error_msg, CAP, "msglib_unlock:%d: %s", line, error); return self->error_msg; }
  }
  return NULL;
}

const char * msgmgr_send(struct msgmgr * self, size_t length) {
  if(self->use_multi_client_lock) return "when use_multi_client_lock is enabled, you must use msgmgr_send_dx";
  return msgmgr_send_dx(self, &self->msg[4], length, &self->msg[4], (uint32_t *)self->msg);
}

struct msgmgr_direct * msgmgr_direct(struct msgmgr * self) {
  return (struct msgmgr_direct *)self->msg;
}

const char * msgmgr_receive(struct msgmgr * self) {
  if(!self->is_server) return "msgmgr_receive is for servers, not clients";
  int line;
  const char * error = msglib_wait(self->msgmgr, self->timeout, &line);
  if(error) { snprintf(self->error_msg, CAP, "msglib_wait:%d: %s", line, error); return self->error_msg; }
  // the server can now use the msgmgr_direct struct to read the message, and write the reply
  return NULL;
}

const char * msgmgr_reply(struct msgmgr * self, size_t length) {
  if(!self->is_server) return "msgmgr_reply is for servers, not clients";
  *(uint32_t *)self->msg = (uint32_t) length;
  int line;
  const char * error = msglib_post(self->msgmgr, &line);
  if(error) { snprintf(self->error_msg, CAP, "msglib_post:%d: %s", line, error); return self->error_msg; }
  return NULL;
}
