// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -fPIC -shared -o neutral.so neutral.c -lrt -pthread
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

struct opaque_data {
  size_t length;
  void * ptr;
  sem_t * notify_server;
  sem_t * notify_client;
  pthread_mutex_t * client_mutex;
  uint8_t * msg;
};

void * msgmgr_connect(const char * name, size_t length, int * error) {
  *error = 0;
  int fd = shm_open(name, O_RDWR, 0); if(fd == -1) { *error = errno; return strerror(errno); }
  void * ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); if(ptr == MAP_FAILED) { *error = errno; return strerror(errno); }
  if(close(fd) == -1) { *error = errno; return strerror(errno); }
  struct opaque_data * data = malloc(sizeof(struct opaque_data));
  data->ptr = ptr;
  data->length = length;
  data->notify_server = ptr;
  data->notify_client = data->notify_server + 1;
  data->client_mutex = (pthread_mutex_t *) (data->notify_client + 1);
  data->msg = (uint8_t *) (data->client_mutex + 1);
  return data;
}

uint8_t * msgmgr_get_mem(void * msgmgr) {
  printf("C %p\n", ((struct opaque_data *) msgmgr)->msg);
  return ((struct opaque_data *) msgmgr)->msg;
}

const char * msgmgr_disconnect(void * msgmgr) {
  struct opaque_data * data = msgmgr;
  if(munmap(data->ptr, data->length) == -1) return strerror(errno);
  free(data);
  return NULL;
}

const char * msgmgr_lock(void * msgmgr) {
  if(pthread_mutex_lock(((struct opaque_data *) msgmgr)->client_mutex)) return "pthread_mutex_lock";
  return NULL;
}

const char * msgmgr_unlock(void * msgmgr) {
  if(pthread_mutex_unlock(((struct opaque_data *) msgmgr)->client_mutex)) return "pthread_mutex_unlock";
  return NULL;
}

const char * msgmgr_post(void * msgmgr) {
  if(sem_post(((struct opaque_data *) msgmgr)->notify_server) == -1) return strerror(errno);
  return NULL;
}

const char * msgmgr_wait(void * msgmgr) {
  struct timespec timeout;
  if(clock_gettime(CLOCK_REALTIME, &timeout) == -1) return strerror(errno);
  timeout.tv_sec += 3;
  if(sem_timedwait(((struct opaque_data *) msgmgr)->notify_client, &timeout) == -1) return strerror(errno);
  return NULL;
}
