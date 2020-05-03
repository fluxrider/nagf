#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

import os
import stat
import mmap
import ctypes
librt = ctypes.CDLL("librt.so", use_errno=True)

# connect to message manager
length = 1024
name = "/prototype-server"
fd = librt.shm_open(name.encode(), ctypes.c_int(os.O_RDWR), ctypes.c_ushort(0))
if fd == -1: raise RuntimeError(f'shm_open: {os.strerror(ctypes.get_errno())}')

# TODO how am I going to be able to have semaphore and mutex from the mmap ?

#void * ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); if(ptr == MAP_FAILED) { perror("mmap"); exit(1); }
#if(close(fd) == -1) { perror("close"); exit(1); }
#sem_t * notify_server = ptr;
#sem_t * notify_client = notify_server + 1;
#pthread_mutex_t * client_mutex = (pthread_mutex_t *) (notify_client + 1);
#uint8_t * msg = (uint8_t *) (client_mutex + 1);

# message server the stuff we read from stdin
#for(int i = 0; i < 4; i++) {
#  // NOTE in this example, I lock the mutex before blocking in scanf. This is pretty horrid.
#  //      also, in the event of a failure or Ctrl-C, the mutex is never unlocked.
#  pthread_mutex_lock(client_mutex);
#  scanf("%s", msg);
#  if(sem_post(notify_server) == -1) { perror("sem_post"); exit(EXIT_FAILURE); }
#  // wait for reply (in this example, wait up to 3 seconds)
#  struct timespec timeout;
#  if(clock_gettime(CLOCK_REALTIME, &timeout) == -1) { perror("clock_gettime"); exit(EXIT_FAILURE); }
#  timeout.tv_sec += 3;
#  if(sem_timedwait(notify_client, &timeout) == -1) { perror("sem_timedwait"); exit(EXIT_FAILURE); }
#  printf("Message received %d\n", msg[0]);
#  pthread_mutex_unlock(client_mutex);
#}

# disconnect from message manager
#if(munmap(ptr, length) == -1) { perror("munmap"); exit(1); }
