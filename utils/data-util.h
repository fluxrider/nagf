#pragma once
// Copyright 2020 David Lareau. This source code form is subject to the terms of the Mozilla Public License 2.0.
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#pragma GCC diagnostic ignored "-Wint-conversion"

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
  size_t has_cache;
};

void dict_init(struct dict * self, size_t memcpy_size, bool key_str, bool dup_str);
void dict_free(struct dict * self);
void dict_set(struct dict * self, intptr_t key, intptr_t val);
intptr_t dict_get(struct dict * self, intptr_t key);
intptr_t dict_get_by_index(struct dict * self, size_t i);
bool dict_has(struct dict * self, intptr_t key);
