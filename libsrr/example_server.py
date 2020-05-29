# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B example_server.py

from srr import Srr
import time

with Srr('/example-srr', is_server=True) as server:
  data = server.receive()
  print(len(data))
  print(data)
  print(bytearray(data).decode())
  server.reply("whatever".encode())

  time.sleep(1) # give client some time before destroying channel
