#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B example_client.py

from msgmgr import MsgMgr

with MsgMgr('/example-msgmgr') as client:
  data = client.send("hello".encode())
  print(len(data))
  print(data)
  print(bytearray(data).decode())
