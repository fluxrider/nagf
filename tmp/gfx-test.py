# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=libsrr python -B ./tmp/gfx-test.py

import time
import importlib
srr = importlib.import_module('libsrr.srr')
gfx_reply = importlib.import_module('utils.gfx-util').GfxReply(True)

with srr.srr('/gfx-swing') as gfx_sync, open('gfx-swing.fifo', 'w') as gfx:
  # load some resources
  face = 'res/face.png'
  font = '/usr/share/fonts/TTF/DejaVuSans.ttf'
  print(f'cache {face}', file=gfx, flush=True)
  print(f'cache {font} 16', file=gfx, flush=True)
  print('title gfx-test', file=gfx, flush=True)

  # only continue once essential resources are loaded
  while True:
    gfx_reply.set(gfx_sync.send(f'stat {face} stat {font}'.encode()))
    face_stat = gfx_reply.stat()
    font_stat = gfx_reply.stat()
    # if any of the stat had errors, reading it will raise the exception
    if face_stat[0] == 'ready' and font_stat[0] == 'ready': break
    print('loading...')
    print(face_stat[0])
    print(font_stat[0])

  # game loop
  fps = ('fps', 0)
  while True:
    print(f'draw {face} 0 0', file=gfx, flush=True)
    print(f'text {font} 16 100 100 ff0000 ff0000 {fps[1]}', file=gfx, flush=True)
    print('flush', file=gfx, flush=True)
    gfx_reply.set(gfx_sync.send('flush fps'.encode()))
    fps = gfx_reply.stat()
