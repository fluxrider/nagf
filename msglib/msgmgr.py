#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

import os
import sys
import ctypes
msglib = ctypes.CDLL("msglib.so", use_errno=True)
msglib.msglib_connect.restype = ctypes.c_void_p
msglib.msglib_get_mem.restype = ctypes.POINTER(ctypes.c_ubyte)
msglib.msglib_wait.restype = ctypes.c_char_p
msglib.msglib_post.restype = ctypes.c_char_p
msglib.msglib_disconnect.restype = ctypes.c_char_p
msglib.msglib_lock.restype = ctypes.c_char_p
msglib.msglib_unlock.restype = ctypes.c_char_p

class MsgMgr:
  def __init__(self, name, length=8192, is_server=False, use_multi_client_lock=False, timeout=3):
    self.name = name
    self.length = length
    self.is_server = is_server
    self.use_multi_client_lock = use_multi_client_lock
    self.timeout = timeout
    error = ctypes.c_int()
    line = ctypes.c_int()
    self.msgmgr = ctypes.c_void_p(msglib.msglib_connect(name.encode(), length, is_server, ctypes.pointer(error), ctypes.pointer(line)))
    if error.value != 0: raise RuntimeError(f'msglib_connect:{line.value}: {os.strerror(error.value)}')
    self.msg = msglib.msglib_get_mem(self.msgmgr)

  def disconnect(self):
    line = ctypes.c_int()
    error = msglib.msglib_disconnect(self.msgmgr, ctypes.pointer(line))
    if error: raise RuntimeError(f'msglib_disconnect:{line.value}: {error.decode()}')

  # client interface (i.e. send a message and wait for a reply from the server)
  def send(self, data):
    if self.is_server: raise RuntimeError()
    line = ctypes.c_int()
    # lock
    if self.use_multi_client_lock:
      error = msglib.msglib_lock(self.msgmgr, ctypes.pointer(line))
      if error: raise RuntimeError(f'msglib_lock:{line.value}: {error.decode()}')
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
    error = msglib.msglib_post(self.msgmgr, ctypes.pointer(line))
    if error: raise RuntimeError(f'msglib_post:{line.value}: {error.decode()}')
    # wait for the reply
    error = msglib.msglib_wait(self.msgmgr, ctypes.c_double(self.timeout), ctypes.pointer(line))
    if error: raise RuntimeError(f'msglib_wait:{line.value}: {error.decode()}')
    # read header
    length = int.from_bytes(self.msg[:4], byteorder=sys.byteorder, signed=False)
    # copy data (see slicing comment in receive() TODO avoid the copy if no multi-client support)
    data = self.msg[4:length+4]
    # unlock
    if self.use_multi_client_lock:
      error = msglib.msglib_unlock(self.msgmgr, ctypes.pointer(line))
      if error: raise RuntimeError(f'msglib_unlock:{line.value}: {error.decode()}')
    return data

  # server interface (i.e. receive a message from the client, process, then reply)
  def receive(self):
    if not self.is_server: raise RuntimeError()
    line = ctypes.c_int()
    error = msglib.msglib_wait(self.msgmgr, ctypes.c_double(self.timeout), ctypes.pointer(line))
    if error: raise RuntimeError(f'msglib_wait:{line.value}: {error.decode()}')
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
    error = msglib.msglib_post(self.msgmgr, ctypes.pointer(line))
    if error: raise RuntimeError(f'msglib_post:{line.value}: {error.decode()}')

  # context managers interface
  def __enter__ (self):
    return self

  def __exit__ (self, type, value, tb):
    self.disconnect()
