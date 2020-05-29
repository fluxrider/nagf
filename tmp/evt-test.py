# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=libsrr python -B ./tmp/evt-test.py

import time
import importlib
Evt = importlib.import_module('utils.evt-util')

with Evt.Evt('/evt-libevdev') as evt:
  command = ''
  while True:
    evt.poll(command)
    if evt.pressed(Evt.UP): print("UP just pressed")
    if evt.released(Evt.F):
      print('Going in no focus mode')
      command = 'no-focus-mode'
    if evt.held(Evt.P): print("P held")
    if evt.released(Evt.ESC):
      print("ESC released")
      break
    if evt.pressed(Evt.G0_EAST): print("G0_EAST just pressed")
    if evt.held(Evt.G0_SOUTH): print("G0_SOUTH held")
    if evt.held(Evt.M): print(evt.mouse())
    if evt.held(Evt.G): print(evt.axis_and_triggers())
    if evt.held(Evt.H): print(evt.histokey())
    if evt.held(Evt.L): print(evt.left_axis())
    if evt.held(Evt.T): print(evt.right_trigger())
    if evt.pressed(Evt.G0_R2): print("G0_R2 just pressed")
    if evt.released(Evt.G0_R2): print("G0_R2 just released")
    if evt.pressed(Evt.G0_R1): print("G0_R1 just pressed")
    if evt.released(Evt.G0_R1): print("G0_R1 just released")

