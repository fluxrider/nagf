#pragma once
// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// dictionnary data structure
// - key must be comparable with < > == or with strcmp and fit in a intptr_t

struct dict {
  size_t memcpy_size; // if non-zero, we deepcopy the value, assuming it is a pointer
  bool key_str; // strcmp key when true, <, >, == when false
  bool dup_str; // if set, we keep a strdup() of all keys
  size_t capacity;
  size_t size;
  intptr_t * keys;
  uint8_t * vals;
};

void dict_init(struct dict * self, size_t memcpy_size, bool key_str, bool dup_str);
void dict_free(struct dict * self);
void dict_set(struct dict * self, intptr_t key, intptr_t val);
void * dict_get(struct dict * self, intptr_t key);