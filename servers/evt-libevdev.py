#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
import os
import sys
import time
import fcntl
import libevdev
import threading
import watchdog.observers
import watchdog.events

has_error = threading.Event()
error = None

# thread that listens to a device
def handle_device(path):
  global error
  global has_error
  try:
    fd = open(path, 'rb')
    fcntl.fcntl(fd, fcntl.F_SETFL, os.O_NONBLOCK)
    device = libevdev.Device(fd)
    print(f'Listening to events of {device.name}')
    while True:
      for evt in device.events():
        print(evt)
  except Exception as e:
    # lost device is not a fatal error
    if not isinstance(e, OSError) or e.errno != 19:
      error = e
      has_error.set()

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

# wait until an error happens
has_error.wait()
print(f'{str(error)}')
