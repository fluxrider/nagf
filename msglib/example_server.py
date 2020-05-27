#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# LD_LIBRARY_PATH=. python -B example_server.py

# filename based import because the normal import:
# - has issues when both modulename.so and modulename.py exist [and I like my filenames]
# - has difficulty with filenames that cannot be variable names [and I like my filenames]
# - requires __init__.py [and I dislike unnecessary files]
def import_dx(path):
  import importlib.util
  spec = importlib.util.spec_from_file_location('what_is_name_for', path)
  mod = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(mod)
  return mod
msgmgr = import_dx('msgmgr.py')

with msgmgr.MsgMgr('/example-msgmgr', is_server=True) as server:
  data = server.receive()
  print(len(data))
  print(data)
  print(bytearray(data).decode())
  server.reply("whatever".encode())
