// Copyright 2020 David Lareau. This source code form is subject to the terms of the Mozilla Public License 2.0.
#define _GNU_SOURCE // for reallocarray on raspberry pi OS which has old libc
#include <stdio.h>
#include <string.h>
#include "data-util.h"

static size_t _find(struct dict * self, intptr_t key) {
  // TODO binary search on large array, even smaller threshold if strcmp
  size_t i = self->size;
  if(self->key_str) while(i > 0 && strcmp(self->keys[i-1], key) >= 0) i--;
  else while(i > 0 && self->keys[i-1] >= key) i--;
  return i;
  // returns the index where key should be/go
}

void dict_init(struct dict * self, size_t memcpy_size, bool key_str, bool dup_str) {
  self->memcpy_size = memcpy_size;
  self->key_str = key_str;
  self->dup_str = dup_str;
  self->capacity = 4;
  self->size = 0;
  self->keys = reallocarray(NULL, self->capacity, sizeof(intptr_t));
  self->vals = reallocarray(NULL, self->capacity, self->memcpy_size? self->memcpy_size : sizeof(intptr_t));
  self->has_cache = -1;
}

void dict_free(struct dict * self) {
  if(self->dup_str) {
    for(int i = 0; i < self->size; i++) {
      free(self->keys[i]);
    }
  }
  free(self->keys);
  free(self->vals);
}

void dict_set(struct dict * self, intptr_t key, intptr_t val) {
  // create a record for this key unless it's already there
  size_t i = _find(self, key);
  if(i == self->size || (self->key_str? strcmp(self->keys[i], key) != 0 : self->keys[i] != key)) {
    // grow
    if(self->size == self->capacity) {
      self->capacity *= 2;
      self->keys = reallocarray(self->keys, self->capacity, sizeof(intptr_t));
      self->vals = reallocarray(self->vals, self->capacity, self->memcpy_size? self->memcpy_size : sizeof(intptr_t));
      if(!self->keys || !self->vals) { printf("out of mem\n"); exit(EXIT_FAILURE); };
    }
    // dup key
    if(self->dup_str) key = strdup(key);
    // shift keys and insert it
    self->size++;
    int above = self->size - i - 1;
    memmove(&self->keys[i+1], &self->keys[i], sizeof(intptr_t) * above);
    self->keys[i] = key;
    // shift values
    if(self->memcpy_size) {
      uint8_t * p_i = self->vals + self->memcpy_size * i;
      memmove(p_i + self->memcpy_size, p_i, self->memcpy_size * above);
    } else {
      intptr_t * v = (intptr_t *)self->vals;
      memmove(&v[i+1], &v[i], sizeof(intptr_t) * above);
    }
  }
  // set value
  if(self->memcpy_size) {
    memcpy(self->vals + self->memcpy_size * i, val, self->memcpy_size);
  } else {
    ((intptr_t *)self->vals)[i] = val;
  }
}

intptr_t dict_get(struct dict * self, intptr_t key) {
  size_t i = -1;
  // check cache
  if(self->has_cache != -1) {
    i = self->has_cache;
    if(i >= self->size || (self->key_str? strcmp(self->keys[i], key) != 0 : self->keys[i] != key)) i = -1;
  }
  // search for it
  if(i == -1) {
    i = _find(self, key);
    if(i == self->size || (self->key_str? strcmp(self->keys[i], key) != 0 : self->keys[i] != key)) return NULL;
  }
  // found
  self->has_cache = -1;
  if(self->memcpy_size) return self->vals + self->memcpy_size * i;
  else return ((intptr_t *)self->vals)[i];
}

intptr_t dict_get_by_index(struct dict * self, size_t i) {
  if(i >= self->size) return NULL;
  if(self->memcpy_size) return self->vals + self->memcpy_size * i;
  else return ((intptr_t *)self->vals)[i];
}

bool dict_has(struct dict * self, intptr_t key) {
  size_t i = _find(self, key);
  if(i < self->size && self->key_str? strcmp(self->keys[i], key) == 0 : self->keys[i] == key) {
    self->has_cache = i;
    return true;
  }
  return false;
}

// TODO zap
