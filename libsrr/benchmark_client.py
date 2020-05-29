# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B benchmark_client.py

from srr import Srr
import sys
import time
import random

seconds = int(sys.argv[1])
bigmsg = 'bigmsg' in sys.argv
multi = 'multi' in sys.argv

with Srr('/benchmark-srr', length=8192, use_multi_client_lock=multi) as client:
  t0 = time.time()
  count = 0
  while time.time() < t0 + seconds:
    x = random.randrange(1000000)
    data = x.to_bytes(4, byteorder=sys.byteorder, signed=False)
    if bigmsg:
      client.msg[:4] = data
      length = client._send(8192)
      reply = client.msg[:length]
    else:
      reply = client.send(data)
    if int.from_bytes(reply, byteorder=sys.byteorder, signed=False) != x + 5:
      print('bad answer')
      break
    count += 1
  print(f'{count / seconds}')
