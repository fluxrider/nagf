#pragma once
// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

#include <stdint.h>
#include <stdbool.h>

void * srr_shm_connect(const char * name, size_t length, bool is_server, int * error, int * line);
uint8_t * srr_shm_get_mem(void * opaque);
const char * srr_shm_disconnect(void * opaque, int * line);
const char * srr_shm_lock(void * opaque, int * line);
const char * srr_shm_unlock(void * opaque, int * line);
const char * srr_shm_post(void * opaque, int * line);
const char * srr_shm_wait(void * opaque, double s, int * line);
