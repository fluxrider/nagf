#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=msglib ./servers/evt-libevdev.py

# A libevdev-based input devices event listener.
# - first use it in 'mapping' mode (i.e. without arg) to spew a mapping file
# - next use it with up to 4 mapping file specified as argument, which are the virtual nagf-gamepads
# - this implementation merges all mice in a single one, making nagf-mouse 2,3 and 4 empty

# Note that the nagf input interface is low level (i.e. is not aware of window nor focus).
# This said, implementations are not required to truly be low level, but this one is,
# so the app has to be careful to ignore inputs if the gfx server report not having focus.

import os
import sys
import fcntl
import libevdev
import threading
import watchdog.observers
import watchdog.events
from msglib.msgmgr import MsgMgr
import bitarray

mapping_mode = len(sys.argv) <= 1

should_quit = threading.Event()
error = None

mutex = threading.Lock()
held = [False] * libevdev.EV_KEY.KEY_MAX.value
pressed = [0] * libevdev.EV_KEY.KEY_MAX.value
released = [False] * libevdev.EV_KEY.KEY_MAX.value

# thread that listens to a device
def handle_device(path):
  global error
  global should_quit
  try:
    fd = open(path, 'rb')
    fcntl.fcntl(fd, fcntl.F_SETFL, os.O_NONBLOCK)
    device = libevdev.Device(fd)
    print(f'Listening to events of {device.name}')
    while True:
      for evt in device.events():
        if mapping_mode:
          print(evt)
        else:
          if evt.matches(libevdev.EV_KEY):
            index = evt.code.value # e.g. 103 for KEY_UP
            mutex.acquire()
            # pressed
            if evt.value == 1:
              held[index] = True
              pressed[index] += 1
            # released
            elif evt.value == 0:
              held[index] = False
              released[index] = True
            mutex.release()

  except Exception as e:
    # lost device is not a fatal error
    if not isinstance(e, OSError) or e.errno != 19:
      error = e
      should_quit.set()

# listen to existing devices
def is_of_interest(path):
  if '-if01-' in path: return False
  if path.endswith('-event-mouse'): return True
  if path.endswith('-event-kbd'): return True
  if path.endswith('-event-joystick'): return True
  return False
folder = '/dev/input/by-id/'
for file in os.listdir(folder):
  path = os.path.join(folder, file)
  if is_of_interest(path):
    threading.Thread(target=handle_device, args=(path,), daemon=True).start()

# monitor for new devices
class MyINotify(watchdog.events.FileSystemEventHandler):
  def on_created(self, event):
    if is_of_interest(event.src_path):
      threading.Thread(target=handle_device, args=(event.src_path,), daemon=True).start()
observer = watchdog.observers.Observer()
observer.schedule(MyINotify(), folder, recursive=False)
observer.start()

# handle client's request for input frame
def handle_client():
  global error
  global should_quit
  try:
    with MsgMgr('/evt-libevdev', is_server=True) as server:
      while(True):
        server.receive()
        bits = bitarray.bitarray()
        # TMP just give up/down/left/right
        mutex.acquire()
        for k in [libevdev.EV_KEY.KEY_UP, libevdev.EV_KEY.KEY_DOWN, libevdev.EV_KEY.KEY_LEFT, libevdev.EV_KEY.KEY_RIGHT]:
          index = k.value
          h = held[index]
          r = released[index]
          p = pressed[index]
          released[index] = False
          pressed[index] = 0
          bits.extend([h, p >= 2, p == 1 or p > 2, r])
        mutex.release()
        server.reply(bits.tobytes())
  except Exception as e:
    error = e
    should_quit.set()
if not mapping_mode:
  threading.Thread(target=handle_client, daemon=True).start()

# wait
print(mapping_mode)
should_quit.wait()
if error: print(f'{str(error)}')
