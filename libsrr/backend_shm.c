// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <time.h>

struct opaque_data {
  size_t length;
  void * ptr;
  sem_t * notify_other;
  sem_t * notify_me;
  pthread_mutex_t * client_mutex;
  pthread_mutexattr_t client_mutex_attr; // it is unclear if it is safe to destroy attr right after pthread_mutex_init, so I carry it.
  uint8_t * msg;
  bool is_server;
  char name[];
};

void * srr_shm_connect(const char * name, size_t length, bool is_server, int * error, int * line) {
  length += sizeof(sem_t) * 2 + sizeof(pthread_mutex_t);
  *error = 0;
  int fd = shm_open(name, O_RDWR | (is_server? O_CREAT : 0), is_server? (S_IRUSR | S_IWUSR) : 0); if(fd == -1) { *line = __LINE__; *error = errno; return strerror(errno); }
  if(is_server) if(ftruncate(fd, length) == -1) { *line = __LINE__; *error = errno; return strerror(errno); }
  void * ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); if(ptr == MAP_FAILED) { *line = __LINE__; *error = errno; return strerror(errno); }
  if(close(fd) == -1) { *line = __LINE__; *error = errno; return strerror(errno); }
  struct opaque_data * data = malloc(sizeof(struct opaque_data) + (is_server? (strlen(name) + 1) : 0));
  data->ptr = ptr;
  data->length = length;
  sem_t * server = ptr;
  sem_t * client = server + 1;
  data->client_mutex = (pthread_mutex_t *)(client + 1);
  if(is_server) {
    if(sem_init(server, 1, 0) == -1) { *line = __LINE__; *error = errno; return strerror(errno); }
    if(sem_init(client, 1, 0) == -1) { *line = __LINE__; *error = errno; return strerror(errno); }
    if(pthread_mutexattr_init(&data->client_mutex_attr)) return "pthread_mutexattr_init";
    if(pthread_mutexattr_setpshared(&data->client_mutex_attr, PTHREAD_PROCESS_SHARED)) return "pthread_mutexattr_setpshared";
    if(pthread_mutex_init(data->client_mutex, &data->client_mutex_attr)) return "pthread_mutex_init";
  }
  data->msg = (uint8_t *) (data->client_mutex + 1);
  data->is_server = is_server;
  data->notify_other = is_server? client : server;
  data->notify_me = is_server? server : client;
  if(is_server) strcpy(data->name, name);
  return data;
}

uint8_t * srr_shm_get_mem(void * opaque) {
  struct opaque_data * data = opaque;
  return data->msg;
}

const char * srr_shm_disconnect(void * opaque, int * line) {
  struct opaque_data * data = opaque;
  if(data->is_server) {
    if(pthread_mutex_destroy(data->client_mutex)) { *line = __LINE__; return "pthread_mutex_destroy"; }
    if(pthread_mutexattr_destroy(&data->client_mutex_attr)) { *line = __LINE__; return "pthread_mutexattr_destroy"; }
    if(sem_destroy(data->notify_me) == -1) { *line = __LINE__; return strerror(errno); }
    if(sem_destroy(data->notify_other) == -1) { *line = __LINE__; return strerror(errno); }
  }
  if(munmap(data->ptr, data->length) == -1) { *line = __LINE__; return strerror(errno); }
  if(data->is_server) if(shm_unlink(data->name) == -1) { *line = __LINE__; return strerror(errno); }
  free(data);
  return NULL;
}

const char * srr_shm_lock(void * opaque, int * line) {
  struct opaque_data * data = opaque;
  if(data->is_server) return "servers should not tamper with the client mutex";
  if(pthread_mutex_lock(data->client_mutex)) return "pthread_mutex_lock";
  return NULL;
}

const char * srr_shm_unlock(void * opaque, int * line) {
  struct opaque_data * data = opaque;
  if(data->is_server) { *line = __LINE__; return "servers should not tamper with the client mutex"; }
  if(pthread_mutex_unlock(data->client_mutex)) { *line = __LINE__; return "pthread_mutex_unlock"; }
  return NULL;
}

const char * srr_shm_post(void * opaque, int * line) {
  struct opaque_data * data = opaque;
  if(sem_post(data->notify_other) == -1) { *line = __LINE__; return strerror(errno); }
  return NULL;
}


#define MY_COPY_OF_NSEC_PER_SEC 1000000000L
void my_copy_of_set_normalized_timespec_from_linux_source(struct timespec *ts, time_t sec, int64_t nsec)
{
	while (nsec >= MY_COPY_OF_NSEC_PER_SEC) {
		/*
		 * The following asm() prevents the compiler from
		 * optimising this loop into a modulo operation. See
		 * also __iter_div_u64_rem() in include/linux/time.h
		 */
		asm("" : "+rm"(nsec));
		nsec -= MY_COPY_OF_NSEC_PER_SEC;
		++sec;
	}
	while (nsec < 0) {
		asm("" : "+rm"(nsec));
		nsec += MY_COPY_OF_NSEC_PER_SEC;
		--sec;
	}
	ts->tv_sec = sec;
	ts->tv_nsec = nsec;
}
const char * srr_shm_wait(void * opaque, double s, int * line) {
  struct opaque_data * data = opaque;
  if(s == 0) if(sem_wait(data->notify_me) == -1) { *line = __LINE__; return strerror(errno); }
  struct timespec timeout;
  if(clock_gettime(CLOCK_REALTIME, &timeout) == -1) { *line = __LINE__; return strerror(errno); }
  my_copy_of_set_normalized_timespec_from_linux_source(&timeout, timeout.tv_sec + (time_t)s, timeout.tv_nsec + (s - (time_t)s) * 1000000000L);
  if(sem_timedwait(data->notify_me, &timeout) == -1) { *line = __LINE__; return strerror(errno); }
  return NULL;
}
