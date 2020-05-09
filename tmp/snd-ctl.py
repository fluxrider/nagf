#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# send message to snd server fifo specified as cmd arg

import os
import sys
import stat
import time
import random
import contextlib

# NOTE aside from this check, you might as well just echo '' >> .fifo, but then again if [ -p "$pipe" ]
server_fifo_path = sys.argv[1]
if not stat.S_ISFIFO(os.stat(server_fifo_path).st_mode): raise Exception("not a fifo")

with open(server_fifo_path, 'w') as f:

  # test basic functionality
  print("stream test_res/music.ogg", file=f, flush=True)
  time.sleep(2.4)
  print("volume .7 1", file=f, flush=True)
  time.sleep(2.4)
  print("stop", file=f, flush=True)
  time.sleep(1)
  print("fire test_res/go.ogg 1 1", file=f, flush=True)
  time.sleep(1)
  print("fire test_res/go.ogg .7 1", file=f, flush=True)
  time.sleep(1)

  # prepare a fifo for pcm data and tell server about it
  pcm_fifo_path = 'pcm.fifo'
  with contextlib.suppress(FileNotFoundError): os.remove(pcm_fifo_path)
  os.mkfifo(pcm_fifo_path)
  print(f'raw {pcm_fifo_path} 1 1', file=f, flush=True)
  
  time.sleep(2)

  # feed pcm data
  samples_per_second = 8000
  with open(pcm_fifo_path, 'wb') as pcm:
    overshoot = 0
    for k in range(60 * 3):
      # prepare the square wave
      hz = random.randrange(150, 250)
      n = int((samples_per_second / hz) / 2)
      data = []
      for i in range(0, n):
        data.append(127)
      for i in range(0, n):
        data.append(129) # -127
      # write it for ~16ms
      N = samples_per_second / 60
      N -= overshoot
      print(f'debug {N} {n} {hz}')
      while N > 0:
        pcm.write(bytearray(data))
        N -= n
        overshoot = abs(N)
      pcm.flush()
      time.sleep(1 / 60)

  #print("shutdown", file=f, flush=True)
