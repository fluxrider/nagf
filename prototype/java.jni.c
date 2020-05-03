// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
#include "client.h"
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

JNIEXPORT jobject JNICALL Java_client_msgmgr_1connect(JNIEnv * env, jclass _class, jstring jstring_name, jlong length) {
  const char * error = NULL;
  const char * name = (*env)->GetStringUTFChars(env, jstring_name, NULL);
  int fd = shm_open(name, O_RDWR, 0); if(fd == -1) error = strerror(errno);
  (*env)->ReleaseStringUTFChars(env, jstring_name, name);

  if(!error) {
    void * ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); if(ptr == MAP_FAILED) error = strerror(errno);
    if(!error) {
      if(close(fd) == -1) error = strerror(errno);
      if(!error) {
        struct opaque_data * data = malloc(sizeof(struct opaque_data));
        data->ptr = ptr;
        data->length = length;
        data->notify_server = ptr;
        data->notify_client = data->notify_server + 1;
        data->client_mutex = (pthread_mutex_t *) (data->notify_client + 1);
        data->msg = (uint8_t *) (data->client_mutex + 1);
        return (*env)->NewDirectByteBuffer(env, data, sizeof(struct opaque_data));
      }
    }
  }

  return (*env)->NewStringUTF(env, error);
}

JNIEXPORT jobject JNICALL Java_client_msgmgr_1get_1mem(JNIEnv * env, jclass _class, jobject msgmgr) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, msgmgr);
  return (*env)->NewDirectByteBuffer(env, data->msg, data->length);
}

JNIEXPORT jstring JNICALL Java_client_msgmgr_1disconnect(JNIEnv * env, jclass _class, jobject msgmgr) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, msgmgr);
  if(munmap(data->ptr, data->length) == -1) return (*env)->NewStringUTF(env, strerror(errno));
  return NULL;
}

JNIEXPORT jstring JNICALL Java_client_msgmgr_1lock(JNIEnv * env, jclass _class, jobject msgmgr) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, msgmgr);
  if(pthread_mutex_lock(data->client_mutex)) return (*env)->NewStringUTF(env, "pthread_mutex_lock failed");
  return NULL;
}

JNIEXPORT jstring JNICALL Java_client_msgmgr_1unlock(JNIEnv * env, jclass _class, jobject msgmgr) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, msgmgr);
  if(pthread_mutex_unlock(data->client_mutex)) return (*env)->NewStringUTF(env, "pthread_mutex_unlock failed");
  return NULL;
}

JNIEXPORT jstring JNICALL Java_client_msgmgr_1post(JNIEnv * env, jclass _class, jobject msgmgr) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, msgmgr);
  if(sem_post(data->notify_server) == -1) return (*env)->NewStringUTF(env, strerror(errno));
  return NULL;
}

JNIEXPORT jstring JNICALL Java_client_msgmgr_1wait(JNIEnv * env, jclass _class, jobject msgmgr) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, msgmgr);
  struct timespec timeout;
  if(clock_gettime(CLOCK_REALTIME, &timeout) == -1) return (*env)->NewStringUTF(env, strerror(errno));
  timeout.tv_sec += 3;
  if(sem_timedwait(data->notify_client, &timeout) == -1) return (*env)->NewStringUTF(env, strerror(errno));
  return NULL;
}
