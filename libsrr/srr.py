#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

import os
import sys
import ctypes
so = ctypes.CDLL("libsrrshm.so", use_errno=True)
so.srr_shm_connect.restype = ctypes.c_void_p
so.srr_shm_get_mem.restype = ctypes.POINTER(ctypes.c_ubyte)
so.srr_shm_wait.restype = ctypes.c_char_p
so.srr_shm_post.restype = ctypes.c_char_p
so.srr_shm_disconnect.restype = ctypes.c_char_p
so.srr_shm_lock.restype = ctypes.c_char_p
so.srr_shm_unlock.restype = ctypes.c_char_p

class Srr:
  def __init__(self, name, length=8192, is_server=False, use_multi_client_lock=False, timeout=3):
    self.name = name
    self.length = length
    self.is_server = is_server
    self.use_multi_client_lock = use_multi_client_lock
    self.timeout = timeout
    error = ctypes.c_int()
    line = ctypes.c_int()
    self.shm = ctypes.c_void_p(so.srr_shm_connect(name.encode(), length, is_server, ctypes.pointer(error), ctypes.pointer(line)))
    if error.value != 0: raise RuntimeError(f'srr_shm_connect:{line.value}: {os.strerror(error.value)}')
    self.msg = so.srr_shm_get_mem(self.shm)

  def disconnect(self):
    line = ctypes.c_int()
    error = so.srr_shm_disconnect(self.shm, ctypes.pointer(line))
    if error: raise RuntimeError(f'srr_shm_disconnect:{line.value}: {error.decode()}')

  # client interface (i.e. send a message and wait for a reply from the server)
  def send(self, data):
    if self.is_server: raise RuntimeError()
    line = ctypes.c_int()
    # lock
    if self.use_multi_client_lock:
      error = so.srr_shm_lock(self.shm, ctypes.pointer(line))
      if error: raise RuntimeError(f'srr_shm_lock:{line.value}: {error.decode()}')
    # header: len(data)
    header = len(data).to_bytes(4, byteorder=sys.byteorder, signed=False)
    self.msg[0] = header[0]
    self.msg[1] = header[1]
    self.msg[2] = header[2]
    self.msg[3] = header[3]
    # body: copy data
    for i in range(len(data)):
      self.msg[4 + i] = data[i]
    # send
    error = so.srr_shm_post(self.shm, ctypes.pointer(line))
    if error: raise RuntimeError(f'srr_shm_post:{line.value}: {error.decode()}')
    # wait for the reply
    error = so.srr_shm_wait(self.shm, ctypes.c_double(self.timeout), ctypes.pointer(line))
    if error: raise RuntimeError(f'srr_shm_wait:{line.value}: {error.decode()}')
    # read header
    length = int.from_bytes(self.msg[:4], byteorder=sys.byteorder, signed=False)
    # copy data (see slicing comment in receive() TODO avoid the copy if no multi-client support)
    data = self.msg[4:length+4]
    # unlock
    if self.use_multi_client_lock:
      error = so.srr_shm_unlock(self.shm, ctypes.pointer(line))
      if error: raise RuntimeError(f'srr_shm_unlock:{line.value}: {error.decode()}')
    return data

  # server interface (i.e. receive a message from the client, process, then reply)
  def receive(self):
    if not self.is_server: raise RuntimeError()
    line = ctypes.c_int()
    error = so.srr_shm_wait(self.shm, ctypes.c_double(self.timeout), ctypes.pointer(line))
    if error: raise RuntimeError(f'srr_shm_wait:{line.value}: {error.decode()}')
    # read header
    length = int.from_bytes(self.msg[:4], byteorder=sys.byteorder, signed=False)
    # return data (in python, slicing a ctypes array seems to do a copy TODO can it be avoided?)
    return self.msg[4:length+4]

  def reply(self, data):
    if not self.is_server: raise RuntimeError()
    # header: len(data)
    header = len(data).to_bytes(4, byteorder=sys.byteorder, signed=False)
    self.msg[0] = header[0]
    self.msg[1] = header[1]
    self.msg[2] = header[2]
    self.msg[3] = header[3]
    # body: copy data
    for i in range(len(data)):
      self.msg[4 + i] = data[i]
    # send
    line = ctypes.c_int()
    error = so.srr_shm_post(self.shm, ctypes.pointer(line))
    if error: raise RuntimeError(f'srr_shm_post:{line.value}: {error.decode()}')

  # context managers interface
  def __enter__(self):
    return self

  def __exit__(self, type, value, tb):
    self.disconnect()
