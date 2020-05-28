#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B benchmark_client.py

from srr import Srr
import sys
import time
import random

with Srr('/benchmark-srr', length=8192) as client:
  seconds = int(sys.argv[1])
  bigmsg = 'bigmsg' in sys.argv
  multi = 'multi' in sys.argv
  t0 = time.time()
  count = 0
  while time.time() < t0 + seconds:
    x = random.randrange(1000000)
    if multi:
      client.send_dx_buffer[:4] = x.to_bytes(4, byteorder=sys.byteorder, signed=False)
      length = client.send_dx(4)
      if int.from_bytes(client.send_dx_buffer[:length], byteorder=sys.byteorder, signed=False) != x + 5:
        print('bad answer')
        break
    else:
      client.srr_direct.contents.msg[:4] = x.to_bytes(4, byteorder=sys.byteorder, signed=False)
      client.send(8192 if bigmsg else 4)
      if int.from_bytes(client.srr_direct.contents.msg[:4], byteorder=sys.byteorder, signed=False) != x + 5:
        print('bad answer')
        break
    count += 1
  print(f'{count} send/receive/reply in {seconds} seconds ({count / seconds} per second)')
