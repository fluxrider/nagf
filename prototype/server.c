// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -o server server.c -lrt -pthread
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
#include <errno.h>
#include <pthread.h>

void main(void) {
  // setup message manager
  size_t length = 1024;
  const char * name = "/prototype-server";
  int fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR); if(fd == -1) { perror("shm_open"); exit(EXIT_FAILURE); }
  if(ftruncate(fd, length) == -1) { perror("ftruncate"); exit(EXIT_FAILURE); }
  void * ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); if(ptr == MAP_FAILED) { perror("mmap"); exit(EXIT_FAILURE); }
  if(close(fd) == -1) { perror("close"); exit(EXIT_FAILURE); }
  sem_t * notify_server = ptr;
  sem_t * notify_client = notify_server + 1;
  pthread_mutex_t * client_mutex = (pthread_mutex_t *) (notify_client + 1);
  if(sem_init(notify_server, 1, 0) == -1) { perror("sem_init"); exit(EXIT_FAILURE); }
  if(sem_init(notify_client, 1, 0) == -1) { perror("sem_init"); exit(EXIT_FAILURE); }
  pthread_mutexattr_t attr;
  if(pthread_mutexattr_init(&attr)) { fprintf(stderr, "pthread_mutexattr_init\n"); exit(EXIT_FAILURE); }
  if(pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)) { fprintf(stderr, "pthread_mutexattr_setpshared\n"); exit(EXIT_FAILURE); }
  if(pthread_mutex_init(client_mutex, &attr)) { fprintf(stderr, "pthread_mutex_init\n"); exit(EXIT_FAILURE); }
  uint8_t * msg = (uint8_t *) (client_mutex + 1);

  // message handling loop (in this example, it breaks if no message is received for 10 seconds)
  int message;
  do {
    struct timespec timeout;
    if(clock_gettime(CLOCK_REALTIME, &timeout) == -1) { perror("clock_gettime"); exit(EXIT_FAILURE); }
    timeout.tv_sec += 10;
    if(sem_timedwait(notify_server, &timeout) == -1) { if(errno == ETIMEDOUT) break; perror("sem_timedwait"); exit(EXIT_FAILURE); }
    // handle message
    message = msg[0]; 
    printf("Message received %d\n", msg[0]);
    msg[0] = rand();
    printf("Replying %d\n", msg[0]);
    if(sem_post(notify_client) == -1) { perror("sem_post"); exit(EXIT_FAILURE); }
  } while(message != 27);
  
  // cleanup message manager
  if(pthread_mutex_destroy(client_mutex)) { fprintf(stderr, "pthread_mutex_destroy\n"); exit(EXIT_FAILURE); }
  if(pthread_mutexattr_destroy(&attr)) { fprintf(stderr, "pthread_mutexattr_destroy\n"); exit(EXIT_FAILURE); }
  if(sem_destroy(notify_client) == -1) { perror("sem_destroy"); exit(EXIT_FAILURE); }
  if(sem_destroy(notify_server) == -1) { perror("sem_destroy"); exit(EXIT_FAILURE); }
  if(munmap(ptr, length) == -1) { perror("munmap"); exit(EXIT_FAILURE); }
  if(shm_unlink(name) == -1) { perror("shm_unlink"); exit(EXIT_FAILURE); }
}


