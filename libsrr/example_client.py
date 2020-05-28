#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B example_client.py

from srr import Srr

with Srr('/example-srr') as client:
  # send
  data = "hello".encode()
  client.srr_direct.contents.msg[:len(data)] = data
  client.send(len(data))
  
  # handle reply
  print(client.srr_direct.contents.length)
  print(client.srr_direct.contents.msg)
  print(bytearray(client.srr_direct.contents.msg[:client.srr_direct.contents.length]).decode())
