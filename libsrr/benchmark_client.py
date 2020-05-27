#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B benchmark_client.py

from srr import Srr
import sys
import time
import random

with Srr('/benchmark-srr') as client:
  seconds = int(sys.argv[1])
  bigmsg = len(sys.argv) >= 3
  bigdata = bytes([0]*(8192-4))
  t0 = time.time()
  count = 0
  while time.time() < t0 + seconds:
    x = random.randrange(1000000)
    data = x.to_bytes(4, byteorder=sys.byteorder, signed=False)
    if bigmsg: data = data + bigdata
    data = client.send(data)
    if int.from_bytes(data, byteorder=sys.byteorder, signed=False) != x + 5:
      print('bad answer')
      break
    count += 1
  print(f'{count} send/receive/reply in {seconds} seconds ({count / seconds} per second)')
