# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=libsrr python -B ./tmp/gfx-test.py

import time
import importlib
srr = importlib.import_module('libsrr.srr')

with srr.srr('/gfx-swing') as gfx:
  while True:
    gfx.send("".encode());
