#pragma once
// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

#include <stdint.h>
#include <stdbool.h>

void * msglib_connect(const char * name, size_t length, bool is_server, int * error, int * line);
uint8_t * msglib_get_mem(void * opaque);
const char * msglib_disconnect(void * opaque, int * line);
const char * msglib_lock(void * opaque, int * line);
const char * msglib_unlock(void * opaque, int * line);
const char * msglib_post(void * opaque, int * line);
const char * msglib_wait(void * opaque, double s, int * line);
