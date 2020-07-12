# Copyright 2020 David Lareau. This program is free software under the terms of the Zero Clause BSD.
# PYTHONPATH=../.. LD_LIBRARY_PATH=../../libsrr python -B car-noise.py

import os
import sys
import stat
import time
import math
import random
import contextlib
import importlib
import threading
import traceback
srr = importlib.import_module('libsrr.srr')
Evt = importlib.import_module('utils.evt-util')
gfx_reply = importlib.import_module('utils.gfx-util').GfxReply(True)

def rotate(x, z, t):
  c = math.cos(t)
  s = math.sin(t)
  return c * x - s * z, s * x + c * z

with open('snd.fifo', 'w') as snd, open('gfx.fifo', 'w') as gfx, Evt.Evt('/car-noise-evt') as evt, srr.srr('/car-noise-gfx') as gfx_sync:
  # setup
  W = 800
  H = 450
  font = '/usr/share/fonts/TTF/DejaVuSans-Bold.ttf'
  print(f'cache {font}', file=gfx, flush=True)
  print('hq', file=gfx, flush=True)
  print('title car-noise', file=gfx, flush=True)
  print(f'window {W} {H} {W} {H}', file=gfx, flush=True)
  
  # states
  x = W / 2
  z = H / 2
  r = 10
  speed = 2 # pixel per seconds
  turn_speed = 3 # angle per seconds (rad)
  facing = 0 # angle (rad)
  acceleration_x = 0
  acceleration_z = 0
  accel_decay = 3
  bounce_loss = .9
  turning_duration = 0 # for squeal purposes
  squealing = False
  closing = False
  tick = 0

  # car noise thread
  def noise():
    global closing
    try:
      shm_path = '/car-noise-snd-1'
      with srr.srr(shm_path, is_server=True) as server:
        print(f'raw {shm_path} 1 1', file=snd, flush=True)
        while not closing:
          N = int.from_bytes(bytearray(server.receive()), byteorder=sys.byteorder, signed=False)
          samples_per_second = 8000
          wave = []
          if squealing:
            hz = 180 - acceleration_lenth/2 + int(tick * 1000) % 20
            n = int((samples_per_second / hz) / 2)
            # triangle wave?
            for i in range(0, n * 2):
              wave.append(int(i * 127 / n))
          else:
            hz = 6 + acceleration_lenth * 1.2
            # square wave
            n = int((samples_per_second / hz) / 2)
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
        server.receive()
        server.reply([])
    except Exception as e:
      print(e)
      traceback.print_tb(e.__traceback__)
      closing = True
  threading.Thread(target=noise, daemon=True).start()

  # game loop
  delta = 0
  focused = True
  while not closing:
    evt.poll('' if focused else 'no-focus-mode')
    closing |= evt.pressed(Evt.ESC)
    
    # rotate
    turning = evt.left() - evt.right()
    facing += turning * -turn_speed * delta
    # acceleration
    dz = 0
    dx = evt.up() - evt.down()
    # if turning, force a small acceleration
    if abs(dx) < .3 and turning != 0: dx = .3
    # reverse driving is slower
    if dx < 0: dx /= 2
    # rotate acceleration direction (facing)
    dx, dz = rotate(dx, dz, facing)
    
    # boost
    if evt.pressed(Evt.G0_EAST):
      dx *= 50
      dz *= 50
    acceleration_x += dx
    acceleration_z += dz
    
    # apply speed
    x += acceleration_x * speed * delta
    z += acceleration_z * speed * delta
    
    # friction (decay)
    acceleration_x -= (acceleration_x * accel_decay * delta)
    acceleration_z -= (acceleration_z * accel_decay * delta)
    
    # tire squeal
    acceleration_lenth = math.sqrt(acceleration_x * acceleration_x + acceleration_z * acceleration_z)
    if turning == 0 or acceleration_lenth < 13 or math.copysign(1, turning) != math.copysign(1, turning_duration):
      turning_duration = 0
    turning_duration += delta * turning
    squealing = abs(turning_duration) > .4

    print(f'fill 808080 0 0 {W} {H}', file=gfx, flush=True)
    print(f'push rotate {x} {z} {facing}', file=gfx, flush=True)
    print(f'fill 0000FF {x - r} {z - r} {2*r} {2*r}', file=gfx, flush=True)
    print(f'fill 00FFFF {x} {z-.5} {r} {1}', file=gfx, flush=True)
    print(f'pop', file=gfx, flush=True)
    if delta > 0: print(f'text {font} 0 0 {W} {H} bottom left {int(H/16)} noclip 0 ffffff 000000 1 ms:{int(delta*1000)} fps:{1.0/delta:.1f}', file=gfx, flush=True)
    print('flush', file=gfx, flush=True)
    gfx_reply.set(gfx_sync.send('delta'.encode()))
    focused = gfx_reply.focused()
    closing |= gfx_reply.closing()
    delta = gfx_reply.stat()[1]
    tick += delta
