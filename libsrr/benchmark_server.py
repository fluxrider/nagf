# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B benchmark_server.py

from srr import Srr
import time
import sys

with Srr('/benchmark-srr', is_server=True, timeout=2, length=8192) as server:
  while True:
    data = server.receive()
    x = int.from_bytes(data[:4], byteorder=sys.byteorder, signed=False)
    x += 5
    server.reply(x.to_bytes(4, byteorder=sys.byteorder, signed=False))
