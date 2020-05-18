#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=msglib ./tmp/evt-test.py

import time
import importlib
Evt = importlib.import_module('utils.evt-util')

with Evt.Evt('/evt-libevdev') as evt:
  while True:
    evt.poll()
    if evt.pressed(Evt.UP): print("UP just pressed")
    if evt.held(Evt.P): print("P held")
    if evt.released(Evt.ESC):
      print("ESC released")
      break
    if evt.pressed(Evt.G0_EAST): print("G0_EAST just pressed")
    if evt.held(Evt.M): print(evt.mouse())
    if evt.held(Evt.G): print(evt.axis_and_triggers())
