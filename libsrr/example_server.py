#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B example_server.py

from srr import Srr
import time

with Srr('/example-srr', is_server=True) as server:
  # receive
  server.receive()
  print(server.srr_direct.contents.length)
  print(server.srr_direct.contents.msg)
  print(bytearray(server.srr_direct.contents.msg[:server.srr_direct.contents.length]).decode())

  # reply
  data = "whatever".encode()
  server.srr_direct.contents.msg[:len(data)] = data
  server.reply(len(data))

  time.sleep(1) # give client some time before destroying channel
