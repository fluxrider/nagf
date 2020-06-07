# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=libsrr python -B ./servers/evt-libevdev.py /test-evt example.map

# A libevdev-based input devices event listener.
# - first use it in 'mapping' mode (i.e. without arg) to spew a mapping file
# - next use it with up to 4 mapping files specified as argument, which are the virtual nagf-gamepads
# - this implementation merges all mice in a single one, making nagf-mouse 2,3 and 4 empty

# Note that the nagf input interface is low level (i.e. is not aware of window nor focus).
# This said, implementations are not required to truly be low level, but this one is,
# so the app has to be careful to ignore inputs if the gfx server report not having focus.

import os
import sys
import time
import fcntl
import libevdev
import traceback
import threading
import watchdog.observers
import watchdog.events
import importlib
srr = importlib.import_module('libsrr.srr')
import bitarray

mapping_count = len(sys.argv) - 2
mapping_mode = mapping_count <= 0
shm_path = sys.argv[1] if not mapping_mode else None
M_COUNT = 4
G_COUNT = 4
G_KEY_COUNT = 17
M_KEY_COUNT = 3
G_AXIS_AND_TRIGGER_COUNT = 6
if mapping_count > G_COUNT: raise RuntimeError("Too many virtual gamepad mapping specified")
print(f'Mapping count: {mapping_count}')
buttons = ['R1', 'R2', 'R3', 'L1', 'L2', 'L3', 'START', 'HOME', 'SELECT', 'NORTH', 'SOUTH', 'EAST', 'WEST', 'UP', 'DOWN', 'LEFT', 'RIGHT']
axes = ['LX', 'LY', 'RX', 'RY']
triggers = ['LT', 'RT']
mapping = []
joystick_only = False

for i in range(mapping_count):
  mapping.append({})
  for b in buttons: mapping[i][b] = set()
  for a in axes: mapping[i][a] = set()
  for t in triggers: mapping[i][t] = set()
  with open(sys.argv[i+2]) as f:
    for line in f:
      line = line.strip()
      parts = line.split()
      k = parts[0]
      if k not in buttons and k not in axes and k not in triggers:
        print(f'BAD MAPPING FILE {sys.argv[i+2]}')
      mapping[i][k].add(line[len(k)+1:])
if mapping_mode:
  mapping.append({})
  for b in buttons: mapping[0][b] = set()
  for a in axes: mapping[0][a] = set()
  for t in triggers: mapping[0][t] = set()

should_quit = threading.Event()
error = None

mutex = threading.Lock()
current_mapping_label = None
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
]
held = [0] * libevdev.EV_KEY.KEY_MAX.value
pressed = [0] * libevdev.EV_KEY.KEY_MAX.value
released = [False] * libevdev.EV_KEY.KEY_MAX.value
mouse = {libevdev.EV_REL.REL_X.value : 0, libevdev.EV_REL.REL_Y.value : 0, libevdev.EV_REL.REL_WHEEL.value : 0}
gamepad_held = []
gamepad_pressed = []
gamepad_released = []
gamepad_abs = []
for i in range(mapping_count):
  gamepad_held.append({})
  gamepad_pressed.append({})
  gamepad_released.append({})
  gamepad_abs.append({})
  for b in buttons:
    gamepad_held[i][b] = 0
    gamepad_pressed[i][b] = 0
    gamepad_released[i][b] = False
  for a in axes:
    gamepad_abs[i][a] = 0
  for t in triggers:
    gamepad_abs[i][t] = 0
histokey = []
histokey_rank = []
histokey_rank.extend(keys)
histokey_rank.extend([None, None, None] * 3) # unsupported mouse buttons
for i in range(G_COUNT):
  for b in buttons:
    histokey_rank.append(f'{b}_{i}')

def cleanup(mutex, touched_key, held, released, touched_gkey, gamepad_held, gamepad_released, touched_abs, gamepad_abs):
  mutex.acquire()
  for index in touched_key:
    if touched_key[index] == 0: continue
    held[index] -= touched_key[index]
    touched_key[index] = 0
    if held[index] == 0: released[index] = True
    if held[index] < 0:
      held[index] = 0
      print("WARNING held count went into negative")
  for i,k in touched_gkey:
    if touched_gkey[(i,k)] == 0: continue
    gamepad_held[i][k] -= touched_gkey[(i,k)]
    touched_gkey[(i,k)] = 0
    if gamepad_held[i][k] == 0: gamepad_released[i][k] = True
    if gamepad_held[i][k] < 0:
      gamepad_held[i][k] = 0
      print("WARNING held count went into negative")
  for i, k in touched_abs: gamepad_abs[i][k] = 0
  mutex.release()



# thread that listens to a device
def handle_device(path):
  global error
  global should_quit
  global mapping
  global joystick_only
  global histokey
  try:
    fd = open(path, 'rb')
    fcntl.fcntl(fd, fcntl.F_SETFL, os.O_NONBLOCK)
    device = libevdev.Device(fd)
    print(f'Listening to events of {device.name}.')
    # identify any virtual gamepad mapping that involves this device
    local_mapping = {}
    if not mapping_mode:
      for i in range(mapping_count):
        for k in mapping[i]:
          for line in mapping[i][k]:
            if line.startswith(path+' '):
              evt_type_code = line[len(path+' '):]
              if evt_type_code not in local_mapping:
                local_mapping[evt_type_code] = []
              local_mapping[evt_type_code].append((k,i))
    # map of things we touched (to revert on disconnect)
    touched_abs = set()
    touched_key = {}
    touched_gkey = {}
    # listen to events
    while True:
      try:
        for evt in device.events():
          if joystick_only and not path.endswith('-event-joystick'):
            # TODO clearing doesn't happen right away because we need an event from the device
            cleanup(mutex, touched_key, held, released, touched_gkey, gamepad_held, gamepad_released, touched_abs, gamepad_abs)
            continue
          if mapping_mode:
            if evt.matches(libevdev.EV_KEY, 1) or evt.matches(libevdev.EV_ABS):
              # ignore weak abs
              if evt.matches(libevdev.EV_ABS):
                info = device.absinfo[evt.code]
                if evt.value <= info.maximum * .7 and evt.value >= info.minimum * .7: continue
              # mapping mode prints device id / event detail for the current mapping going on
              mutex.acquire()
              mapping[0][current_mapping_label].add(f'{path} {evt.type} {evt.code}')
              # TODO hats need value, and axis for button also need value
              mutex.release()
          else:
            # buttons and keys
            if evt.matches(libevdev.EV_KEY):
              if evt.value == 1 or evt.value == 0:
                mutex.acquire()
                try:
                  rank = histokey_rank.index(evt.code)
                  histokey.append((rank,evt.value))
                except ValueError:
                  pass
                index = evt.code.value # e.g. 103 for KEY_UP
                # pressed
                if evt.value == 1:
                  if held[index] == 0: pressed[index] += 1
                  held[index] += 1
                  if index not in touched_key: touched_key[index] = 0
                  touched_key[index] += 1
                # released
                elif evt.value == 0:
                  held[index] -= 1
                  if index not in touched_key or touched_key[index] == 0:
                    print("WARNING released by device, but never held by device")
                  else:
                    touched_key[index] -= 1
                  if held[index] == 0: released[index] = True
                  if held[index] < 0:
                    held[index] = 0
                    print("WARNING held count went into negative")
                mutex.release()
            # mouse x/y/wheel
            if evt.matches(libevdev.EV_REL):
              if evt.code.value in mouse:
                mutex.acquire()
                mouse[evt.code.value] += evt.value
                mutex.release()
            # virtual gamepad
            evt_type_code = f'{evt.type} {evt.code}'
            if evt_type_code in local_mapping:
              info = device.absinfo[evt.code] if evt.matches(libevdev.EV_ABS) else None
              for (k,i) in local_mapping[evt_type_code]:
                if k in buttons:
                  if evt.matches(libevdev.EV_KEY):
                    if evt.value == 1 or evt.value == 0:
                      mutex.acquire()
                      histokey.append((histokey_rank.index(f'{k}_{i}'),evt.value))
                      # pressed
                      if evt.value == 1:
                        if gamepad_held[i][k] == 0: gamepad_pressed[i][k] += 1
                        gamepad_held[i][k] += 1
                        if (i,k) not in touched_gkey: touched_gkey[(i,k)] = 0
                        touched_gkey[(i,k)] += 1
                      # released
                      elif evt.value == 0:
                        gamepad_held[i][k] -= 1
                        if (i,k) not in touched_gkey or touched_gkey[(i,k)] == 0:
                          print("WARNING released by device, but never held by device")
                        else:
                          touched_gkey[(i,k)] -= 1
                        if gamepad_held[i][k] == 0: gamepad_released[i][k] = True
                        if gamepad_held[i][k] < 0:
                          gamepad_held[i][k] = 0
                          print("WARNING held count went into negative")
                      mutex.release()
                  else:
                    # applying an arbitrary deadzone of .2
                    # note that axis mapped on a button causes know discrepency on the held count
                    was = gamepad_held[i][k]
                    h = (evt.value - info.minimum) / (info.maximum - info.minimum) >= .2
                    if h != was:
                      mutex.acquire()
                      histokey.append((histokey_rank.index(f'{k}_{i}'), 1 if h else 0))
                      # pressed
                      if h:
                        gamepad_pressed[i][k] += 1
                        gamepad_held[i][k] += 1
                        if (i,k) not in touched_gkey: touched_gkey[(i,k)] = 0
                        touched_gkey[(i,k)] += 1
                      # released
                      else:
                        gamepad_held[i][k] -= 1
                        if (i,k) not in touched_gkey or touched_gkey[(i,k)] == 0:
                          pass
                        else:
                          touched_gkey[(i,k)] -= 1
                        gamepad_released[i][k] = True
                        if gamepad_held[i][k] < 0: gamepad_held[i][k] = 0
                      mutex.release()

                if k in triggers or k in axes:
                  touched_abs.add((i, k))
                  low = 0 if k in triggers else -127
                  high = 255 if k in triggers else 127
                  mutex.acquire()
                  if info: gamepad_abs[i][k] = int((evt.value - info.minimum) / (info.maximum - info.minimum) * (high - low)) + low
                  else: gamepad_abs[i][k] = int(evt.value * (high - low)) + low
                  if gamepad_abs[i][k] > high:
                    gamepad_abs[i][k] = high
                    print("WARNING somehow axis went out of bound {evt.value}")
                  if gamepad_abs[i][k] < low:
                    gamepad_abs[i][k] = low
                    print("WARNING somehow axis went out of bound {evt.value")
                  mutex.release()
      except libevdev.EventsDroppedException as e:
        print(f'{device.name} dropped some events.')
        cleanup(mutex, touched_key, held, released, touched_gkey, gamepad_held, gamepad_released, touched_abs, gamepad_abs)
        for evt in device.sync():
          print(f'sync {evt}')
  except Exception as e:
    # lost device is not a fatal error
    if not isinstance(e, OSError) or e.errno != 19:
      error = e
      should_quit.set()
    else:
      print(f'Lost device {device.name}.')
      cleanup(mutex, touched_key, held, released, touched_gkey, gamepad_held, gamepad_released, touched_abs, gamepad_abs)

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
  global joystick_only
  global histokey
  try:
    with srr.srr(shm_path, length=256, is_server=True) as server:
      while(True):
        data = server.receive()
        command = "none"
        if len(data) > 0: command = bytearray(data).decode()
        joystick_only = command == 'no-focus-mode'
        bits = bitarray.bitarray()
        mutex.acquire()
        # keys and buttons (but not gamepad's)
        for k in keys:
          index = k.value
          h = held[index]
          r = released[index]
          p = pressed[index]
          released[index] = False
          pressed[index] = 0
          bits.extend([h > 0, p >= 2, p == 1 or p > 2, r])
        # unsupported mice 2,3,4
        for i in range((M_COUNT - 1) * M_KEY_COUNT):
          bits.extend([False, False, False, False])
        # virtual gamepad keys
        for i in range(mapping_count):
          for b in buttons:
            h = gamepad_held[i][b]
            r = gamepad_released[i][b]
            p = gamepad_pressed[i][b]
            gamepad_released[i][b] = False
            gamepad_pressed[i][b] = 0
            bits.extend([h > 0, p >= 2, p == 1 or p > 2, r])
        # missing virtual gamepad keys
        for i in range((G_COUNT - mapping_count) * G_KEY_COUNT):
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
        for i in range((M_COUNT - 1)):
          data.extend([0] * M_KEY_COUNT)
        # virtual gamepads axis and triggers
        for i in range(mapping_count):
          for a in axes:
            data.extend(gamepad_abs[i][a].to_bytes(1, byteorder='little', signed=True))
          for t in triggers:
            data.extend(gamepad_abs[i][t].to_bytes(1, byteorder='little', signed=False))
        # missing virtual gamepads axis and triggers
        for i in range((G_COUNT - mapping_count)):
          data.extend([0] * G_AXIS_AND_TRIGGER_COUNT)
        # 16 chronological key press/release
        for i in range(16):
          v = histokey[i][0] if i < len(histokey) else 255 # where 255 means KEY_NONE
          data.extend(v.to_bytes(1, byteorder='little', signed=False)) 
        histobits = bitarray.bitarray()
        for i in range(16):
          v = histokey[i][1] if i < len(histokey) else 0
          histobits.append(v)
        histokey = []
        mutex.release()
        server.reply(bits.tobytes() + bytes(data) + histobits.tobytes())
  except Exception as e:
    error = e
    should_quit.set()
if not mapping_mode:
  threading.Thread(target=handle_client, daemon=True).start()
  # wait
  should_quit.wait()
  if isinstance(error, Exception): traceback.print_tb(error.__traceback__)
  if error: print(f'ERROR {error}')

# mapping mode
else:
  print("-- Mapping virtual gamepad --")
  print("You will have 3 seconds per button/axis/trigger to map device keys and what not to it.")
  print("GO!")
  for button in buttons:
    if should_quit.is_set(): raise RuntimeError(f'{str(error)}')
    mutex.acquire()
    print(f'Virtual {button} button.')
    current_mapping_label = button
    mutex.release()
    time.sleep(3)
  for axis in axes:
    if should_quit.is_set(): raise RuntimeError(f'{str(error)}')
    mutex.acquire()
    print(f'Virtual {axis} stick.')
    current_mapping_label = axis
    mutex.release()
    time.sleep(3)
  for trigger in triggers:
    if should_quit.is_set(): raise RuntimeError(f'{str(error)}')
    mutex.acquire()
    print(f'Virtual {trigger} trigger.')
    current_mapping_label = trigger
    mutex.release()
    time.sleep(3)
  print("Have a nice day!")
  # dump mapping
  with open('gamepad.map', 'w') as f:
    for k in mapping[0]:
      for m in mapping[0][k]:
        print(f'{k} {m}', file=f)
