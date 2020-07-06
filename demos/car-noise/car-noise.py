# Copyright 2020 David Lareau. This program is free software under the terms of the Zero Clause BSD.
# PYTHONPATH=../.. LD_LIBRARY_PATH=../../libsrr python -B car-noise.py

import os
import sys
import stat
import time
import random
import contextlib
import importlib
srr = importlib.import_module('libsrr.srr')
Evt = importlib.import_module('utils.evt-util')
gfx_reply = importlib.import_module('utils.gfx-util').GfxReply(True)

with open('snd.fifo', 'w') as snd, open('gfx.fifo', 'w') as gfx, Evt.Evt('/car-noise-evt') as evt, srr.srr('/car-noise-gfx') as gfx_sync:
  # setup
  W = 800
  H = 450
  font = '/usr/share/fonts/TTF/DejaVuSans.ttf'
  print(f'cache {font}', file=gfx, flush=True)
  print('hq', file=gfx, flush=True)
  print('title car-noise', file=gfx, flush=True)
  print(f'window {W} {H} {W} {H}', file=gfx, flush=True)

  # game loop
  delta_time = ('delta', 1)
  focused = True
  closing = False
  while not closing:
    evt.poll('' if focused else 'no-focus-mode')
    closing |= evt.pressed(Evt.ESC)
    print(f'fill 808080 0 0 {W} {H}', file=gfx, flush=True)
    print(f'text {font} 0 0 {W} {H} bottom left {int(H/16)} noclip 0 ffffff 000000 1 ms:{int(delta_time[1]*1000)} fps:{1.0/delta_time[1]:.1f}', file=gfx, flush=True)
    print('flush', file=gfx, flush=True)
    gfx_reply.set(gfx_sync.send('delta'.encode()))
    focused = gfx_reply.focused()
    closing |= gfx_reply.closing()
    delta_time = gfx_reply.stat()
