#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=msglib ./tmp/snd-ctl.py snd-gstream.fifo

# send message to snd server fifo specified as cmd arg

import os
import sys
import stat
import time
import random
import contextlib
from msglib.msgmgr import MsgMgr

# NOTE aside from this check, you might as well just echo '' >> .fifo, but then again if [ -p "$pipe" ]
server_fifo_path = sys.argv[1]
if not stat.S_ISFIFO(os.stat(server_fifo_path).st_mode): raise Exception("not a fifo")

with open(server_fifo_path, 'w') as f:

  # test basic functionality
  #print("stream test_res/music.ogg", file=f, flush=True)
  #time.sleep(2.4)
  #print("volume .7 1", file=f, flush=True)
  #time.sleep(2.4)
  #print("stop", file=f, flush=True)
  #time.sleep(1)
  print("fire test_res/go.ogg 1 1", file=f, flush=True)
  time.sleep(1)
  print("fire test_res/go.ogg .7 1", file=f, flush=True)
  time.sleep(1)

  shm_path = '/my-tmp-shm'
  with MsgMgr(shm_path, is_server=True) as server:
    print(f'raw {shm_path} 1 1', file=f, flush=True)
    limit = 8000 * 3
    while limit > 0:
      N = int.from_bytes(bytearray(server.receive()), byteorder=sys.byteorder, signed=False)
      limit -= N
      # feed pcm data
      samples_per_second = 8000
      # prepare the square wave
      hz = 200 #random.randrange(150, 250)
      n = int((samples_per_second / hz) / 2)
      wave = []
      for i in range(0, n):
        wave.append(127)
      for i in range(0, n):
        wave.append(129) # -127
      # write it to fit N (with overshoot)
      data = []
      while N > 0:
        data.extend(wave)
        N -= n
      server.reply(bytearray(data))
      limit -= abs(N)
    server.receive()
    server.reply([])

  #print("shutdown", file=f, flush=True)
