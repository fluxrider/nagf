#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. ./example_server.py

import time
import os
import ctypes
msglib = ctypes.CDLL("msglib.so", use_errno=True)
msglib.msglib_connect.restype = ctypes.c_void_p
msglib.msglib_get_mem.restype = ctypes.POINTER(ctypes.c_byte)
msglib.msglib_wait.restype = ctypes.c_char_p
msglib.msglib_post.restype = ctypes.c_char_p
msglib.msglib_disconnect.restype = ctypes.c_char_p

# connect to message manager
length = 1024
name = "/example-msgmgr"
error = ctypes.c_int()
line = ctypes.c_int()
msgmgr = ctypes.c_void_p(msglib.msglib_connect(name.encode(), length, True, ctypes.pointer(error), ctypes.pointer(line)))
if error.value != 0: raise RuntimeError(f'msglib_connect:{line.value}: {os.strerror(error.value)}')

msg = msglib.msglib_get_mem(msgmgr)

error = msglib.msglib_wait(msgmgr, ctypes.c_double(4), ctypes.pointer(line))
if error: raise RuntimeError(f'msglib_wait:{line.value}: {error.decode()}')

print(msg[0])
msg[0] = 42

error = msglib.msglib_post(msgmgr, ctypes.pointer(line))
if error: raise RuntimeError(f'msglib_post:{line.value}: {error.decode()}')

time.sleep(2)

error = msglib.msglib_disconnect(msgmgr, ctypes.pointer(line))
if error: raise RuntimeError(f'msglib_disconnect:{line.value}: {error.decode()}')
