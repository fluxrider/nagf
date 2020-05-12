#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=msglib ./tmp/evt-test.py

import time
from msglib.msgmgr import MsgMgr

with MsgMgr('/evt-libevdev') as client:
  while True:
    data = client.send([])
    # TMP interpret as up/down/left/right only
    up_held = data[0] >> 7 != 0
    up_pressed = ((data[0] >> 5) & 3)
    up_released = ((data[0] >> 4) & 1) != 0
    print(f'{up_held} {up_pressed} {up_released}')
    # simulate lag to test press count
    time.sleep(1)
