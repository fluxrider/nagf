#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. ./client.py

import os
import stat
import mmap
import ctypes
libmsgmgr = ctypes.CDLL("neutral.so", use_errno=True)
libmsgmgr.msgmgr_connect.restype = ctypes.c_void_p
libmsgmgr.msgmgr_get_mem.restype = ctypes.POINTER(ctypes.c_byte)

# connect to message manager
length = 1024
name = "/prototype-server"
error = ctypes.c_int()
msgmgr = ctypes.c_void_p(libmsgmgr.msgmgr_connect(name.encode(), length, ctypes.pointer(error)))
if error: raise RuntimeError(f'msgmgr_connect: {os.strerror(error.value)}')
msg = libmsgmgr.msgmgr_get_mem(msgmgr)

print(msg)
msg[0] = 23

error = libmsgmgr.msgmgr_lock(msgmgr)
if error: print(error)

error = libmsgmgr.msgmgr_post(msgmgr)
if error: print(error)

error = libmsgmgr.msgmgr_wait(msgmgr)
if error: print(error)

print(msg[0])

error = libmsgmgr.msgmgr_unlock(msgmgr)
if error: print(error)

error = libmsgmgr.msgmgr_disconnect(msgmgr)
if error: print(error)

