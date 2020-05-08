#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# send message to snd server fifo specified as cmd arg

import os
import sys
import stat
import time

# NOTE aside from this check, you might as well just echo '' >> .fifo, but then again if [ -p "$pipe" ]
server_fifo_path = sys.argv[1]
if not stat.S_ISFIFO(os.stat(server_fifo_path).st_mode): raise Exception("not a fifo")

with open(server_fifo_path, 'w') as f:
  print("volume 1 1", file=f, flush=True)
  print("stream test_res/music.ogg", file=f, flush=True)
  time.sleep(2.4)
  print("volume .7 1", file=f, flush=True)
  time.sleep(2.4)
  print("stop", file=f, flush=True)
  time.sleep(2.4)
  print("shutdown", file=f, flush=True)
