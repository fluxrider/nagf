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

class Srr:
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
        ("error_msg", ctypes.c_char_p)
      ]
    self.srr_direct_class = srr_direct
    self.srr_class = srr
    self.srr = srr()
    error = so.srr_init(ctypes.byref(self.srr), name.encode(), length, is_server, use_multi_client_lock, ctypes.c_double(timeout))
    if error: raise RuntimeError(f'srr_init: {error.decode()}')
    so.srr_direct.restype = ctypes.POINTER(srr_direct)
    self.srr_direct = so.srr_direct(ctypes.byref(self.srr))
    send_dx_buffer_type = ctypes.c_ubyte * length
    self.send_dx_buffer = send_dx_buffer_type()

  def disconnect(self):
    error = so.srr_disconnect(ctypes.byref(self.srr))
    if error: raise RuntimeError(f'srr_disconnect: {error.decode()}')

  # client interface (i.e. send a message and wait for a reply from the server)
  def send(self, length):
    # it is assumed that self.srr_direct.contents.msg[:length] was filled by caller
    error = so.srr_send(ctypes.byref(self.srr), length)
    if error: raise RuntimeError(f'srr_send: {error.decode()}')
    # caller can now read self.srr_direct.contents.msg[:self.srr_direct.contents.length]

  def send_dx(self, length):
    # it is assumed that self.send_dx_buffer[:length] was filled by caller
    retval_length = ctypes.c_uint()
    error = so.srr_send_dx(ctypes.byref(self.srr), self.send_dx_buffer, length, self.send_dx_buffer, ctypes.pointer(retval_length))
    if error: raise RuntimeError(f'srr_send_dx: {error.decode()}')
    return int.from_bytes(retval_length, byteorder=sys.byteorder, signed=False)
    # caller can now read self.send_dx_buffer[:retval_length]

  # server interface (i.e. receive a message from the client, process, then reply)
  def receive(self):
    error = so.srr_receive(ctypes.byref(self.srr))
    if error: raise RuntimeError(f'srr_receive: {error.decode()}')
    # caller can now read self.srr_direct.contents.msg[:self.srr_direct.contents.length]

  def reply(self, length):
    # it is assumed that self.srr_direct.contents.msg[:length] was filled by caller
    error = so.srr_reply(ctypes.byref(self.srr), length)
    if error: raise RuntimeError(f'srr_reply: {error.decode()}')

  # context managers interface
  def __enter__(self):
    return self

  def __exit__(self, type, value, tb):
    self.disconnect()
