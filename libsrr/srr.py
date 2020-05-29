#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

import os
import sys
import ctypes
so = ctypes.CDLL("libsrr.so")
so.srr_init.restype = ctypes.c_char_p
so.srr_disconnect.restype = ctypes.c_char_p
so.srr_send_dx.restype = ctypes.c_char_p
so.srr_send.restype = ctypes.c_char_p
so.srr_receive.restype = ctypes.c_char_p
so.srr_reply.restype = ctypes.c_char_p

class srr:
  def __init__(self, name, length=8192, is_server=False, use_multi_client_lock=False, timeout=3):
    class srr_direct(ctypes.Structure):
      _fields_ = [("length", ctypes.c_uint), ("msg", ctypes.c_ubyte * length)]
    class srr(ctypes.Structure):
      _fields_ = [
        ("length", ctypes.c_size_t),
        ("is_server", ctypes.c_bool),
        ("use_multi_client_lock", ctypes.c_bool),
        ("timeout", ctypes.c_double),
        ("shm", ctypes.c_void_p),
        ("msg", ctypes.POINTER(ctypes.c_ubyte)),
        ("error_msg", ctypes.c_char * 512),
        ("closed", ctypes.c_bool)
      ]
    self.srr_direct_class = srr_direct
    self.srr_class = srr
    self.srr = srr()
    error = so.srr_init(ctypes.byref(self.srr), name.encode(), length, is_server, use_multi_client_lock, ctypes.c_double(timeout))
    if error: raise RuntimeError(f'srr_init: {error.decode()}')
    if is_server or not use_multi_client_lock:
      self.direct = True
      so.srr_direct.restype = ctypes.POINTER(srr_direct)
      self.srr_direct = so.srr_direct(ctypes.byref(self.srr))
      self.msg = self.srr_direct.contents.msg
    else:
      self.direct = False
      send_dx_buffer_type = ctypes.c_ubyte * length
      self.send_dx_buffer = send_dx_buffer_type()
      self.msg = self.send_dx_buffer

  def disconnect(self):
    error = so.srr_disconnect(ctypes.byref(self.srr))
    if error: raise RuntimeError(f'srr_disconnect: {error.decode()}')

  # low level client interface (i.e. send a message and wait for a reply from the server)
  def _send(self, length):
    # it is assumed that self.msg[:length] was filled by caller
    if self.direct:
      error = so.srr_send(ctypes.byref(self.srr), length)
      if error: raise RuntimeError(f'srr_send: {error.decode()}')
      return self.srr_direct.contents.length
    else:
      retval_length = ctypes.c_uint()
      error = so.srr_send_dx(ctypes.byref(self.srr), self.send_dx_buffer, length, self.send_dx_buffer, ctypes.pointer(retval_length))
      if error: raise RuntimeError(f'srr_send_dx: {error.decode()}')
      return int.from_bytes(retval_length, byteorder=sys.byteorder, signed=False)
    # caller can now read self.msg[:length]

  # low level server interface (i.e. receive a message from the client, process, then reply)
  def _receive(self):
    error = so.srr_receive(ctypes.byref(self.srr))
    if error: raise RuntimeError(f'srr_receive: {error.decode()}')
    return self.srr_direct.contents.length
    # caller can now read self.msg[:length]

  def _reply(self, length):
    # it is assumed that self.msg[:length] was filled by caller
    error = so.srr_reply(ctypes.byref(self.srr), length)
    if error: raise RuntimeError(f'srr_reply: {error.decode()}')
    
  # high level interface
  def send(self, data):
    self.msg[:len(data)] = data
    return self.msg[:self._send(len(data))]

  def receive(self):
    return self.msg[:self._receive()]

  def reply(self, data):
    self.msg[:len(data)] = data
    self._reply(len(data))

  # context managers interface
  def __enter__(self):
    return self

  def __exit__(self, type, value, tb):
    self.disconnect()
