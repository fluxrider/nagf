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
keys = [
  libevdev.EV_KEY.KEY_A, libevdev.EV_KEY.KEY_B, libevdev.EV_KEY.KEY_C, libevdev.EV_KEY.KEY_D, libevdev.EV_KEY.KEY_E, libevdev.EV_KEY.KEY_F, libevdev.EV_KEY.KEY_G, libevdev.EV_KEY.KEY_H, libevdev.EV_KEY.KEY_I, libevdev.EV_KEY.KEY_J, libevdev.EV_KEY.KEY_K, libevdev.EV_KEY.KEY_L, libevdev.EV_KEY.KEY_M, libevdev.EV_KEY.KEY_N, libevdev.EV_KEY.KEY_O, libevdev.EV_KEY.KEY_P, libevdev.EV_KEY.KEY_Q, libevdev.EV_KEY.KEY_R, libevdev.EV_KEY.KEY_S, libevdev.EV_KEY.KEY_T, libevdev.EV_KEY.KEY_U, libevdev.EV_KEY.KEY_V, libevdev.EV_KEY.KEY_W, libevdev.EV_KEY.KEY_X, libevdev.EV_KEY.KEY_Y, libevdev.EV_KEY.KEY_Z,
  libevdev.EV_KEY.KEY_0, libevdev.EV_KEY.KEY_1, libevdev.EV_KEY.KEY_2, libevdev.EV_KEY.KEY_3, libevdev.EV_KEY.KEY_4, libevdev.EV_KEY.KEY_5, libevdev.EV_KEY.KEY_6, libevdev.EV_KEY.KEY_7, libevdev.EV_KEY.KEY_8, libevdev.EV_KEY.KEY_9,
  libevdev.EV_KEY.KEY_UP, libevdev.EV_KEY.KEY_DOWN, libevdev.EV_KEY.KEY_LEFT, libevdev.EV_KEY.KEY_RIGHT,
  libevdev.EV_KEY.KEY_ESC,
  libevdev.EV_KEY.KEY_TAB,
  libevdev.EV_KEY.KEY_LEFTALT,
  libevdev.EV_KEY.KEY_RIGHTALT,
  libevdev.EV_KEY.KEY_LEFTCTRL,
  libevdev.EV_KEY.KEY_RIGHTCTRL,
  libevdev.EV_KEY.KEY_LEFTSHIFT,
  libevdev.EV_KEY.KEY_RIGHTSHIFT,
  libevdev.EV_KEY.KEY_SPACE,
  libevdev.EV_KEY.KEY_F1, libevdev.EV_KEY.KEY_F2, libevdev.EV_KEY.KEY_F3, libevdev.EV_KEY.KEY_F4, libevdev.EV_KEY.KEY_F5, libevdev.EV_KEY.KEY_F6, libevdev.EV_KEY.KEY_F7, libevdev.EV_KEY.KEY_F8, libevdev.EV_KEY.KEY_F9, libevdev.EV_KEY.KEY_F10, libevdev.EV_KEY.KEY_F11, libevdev.EV_KEY.KEY_F12,
  libevdev.EV_KEY.KEY_ENTER,
  libevdev.EV_KEY.KEY_BACKSPACE,
  libevdev.EV_KEY.KEY_PAGEUP, libevdev.EV_KEY.KEY_PAGEDOWN,
  libevdev.EV_KEY.KEY_GRAVE, libevdev.EV_KEY.KEY_LEFTBRACE, libevdev.EV_KEY.KEY_RIGHTBRACE, libevdev.EV_KEY.KEY_DOT, libevdev.EV_KEY.KEY_SEMICOLON, libevdev.EV_KEY.KEY_APOSTROPHE, libevdev.EV_KEY.KEY_BACKSLASH, libevdev.EV_KEY.KEY_SLASH, libevdev.EV_KEY.KEY_COMMA,
  libevdev.EV_KEY.BTN_LEFT, libevdev.EV_KEY.BTN_RIGHT, libevdev.EV_KEY.BTN_MIDDLE,
  None, None, None, # mouse 2
  None, None, None, # mouse 3
  None, None, None, # mouse 4
  None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, # gamepad 1
  None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, # gamepad 2
  None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, # gamepad 3
  None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None, None # gamepad 4
]
held = [False] * libevdev.EV_KEY.KEY_MAX.value
pressed = [0] * libevdev.EV_KEY.KEY_MAX.value
released = [False] * libevdev.EV_KEY.KEY_MAX.value
mouse = {libevdev.EV_REL.REL_X.value : 0, libevdev.EV_REL.REL_Y.value : 0, libevdev.EV_REL.REL_WHEEL.value : 0}

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
          # buttons and keys
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
          # mouse x/y/wheel
          if evt.matches(libevdev.EV_REL):
            if evt.code.value in mouse:
              mutex.acquire()
              mouse[evt.code.value] += evt.value
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
        # keys and buttons
        for k in keys:
          if k:
            index = k.value
            h = held[index]
            r = released[index]
            p = pressed[index]
            released[index] = False
            pressed[index] = 0
            bits.extend([h, p >= 2, p == 1 or p > 2, r])
          else:
            bits.extend([False, False, False, False])
        # mouse
        data = []
        for k in mouse:
          if mouse[k] > 127: mouse[k] = 127
          elif mouse[k] < -127: mouse[k] = -127
        data.extend(mouse[libevdev.EV_REL.REL_X.value].to_bytes(1, byteorder='little', signed=True))
        data.extend(mouse[libevdev.EV_REL.REL_Y.value].to_bytes(1, byteorder='little', signed=True))
        data.extend(mouse[libevdev.EV_REL.REL_WHEEL.value].to_bytes(1, byteorder='little', signed=True))
        for k in mouse: mouse[k] = 0
        # empty mouse 2, 3 and 4
        data.extend([0, 0, 0])
        data.extend([0, 0, 0])
        data.extend([0, 0, 0])
        # virtual nagf-gamepads
        mutex.release()
        server.reply(bits.tobytes() + bytes(data))
  except Exception as e:
    error = e
    should_quit.set()
if not mapping_mode:
  threading.Thread(target=handle_client, daemon=True).start()

# wait
should_quit.wait()
if error: print(f'{str(error)}')
