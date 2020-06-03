# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

import sys

class GfxReply:
  def __init__(self, throw):
    self.data = None
    self.throw = throw

  def set(self, data):
    self.data = data
    self.stat_p = 9
    print(f'gfx-util: len={len(data)}')
    print(data)

  def focused(self):
    return data[0] != 0

  def width(self):
    return self._utils_read_int(1)

  def height(self):
    return self._utils_read_int(5)

  def stat(self):
    retval = self._stat()
    if retval[0] == 'error': raise RuntimeError(retval[1])
    return retval

  def _stat(self):
    # read first byte, which tells us the type of the stat
    t = self.data[self.stat_p]
    self.stat_p += 1
    # error type
    if t == 0:
      code = self.data[self.stat_p:self.stat_p+3]
      self.stat_p += 3
      return ('error', f'{chr(code[0])}{chr(code[1])}{chr(code[2])}')
    # img type
    if t == 1:
      w = self._utils_read_int(self.stat_p)
      h = self._utils_read_int(self.stat_p+4)
      self.stat_p += 8
      if w == 0: return ('loading', f'{h/1000}')
      return ('ready', w, h)
    # font metrics TODO not sure what this entails yet
    if t == 2:
      #tmp = self._utils_read_int(self.stat_p)
      #self.stat_p += 4
      #if tmp == 0:
      #  loading = self._utils_read_int(self.stat_p)
      #  self.stat_p += 4
      #  return ('loading', f'{loading/1000}')
      return ('ready',)
    # fps
    if t == 3:
      fps = self._utils_read_int(self.stat_p)
      self.stat_p += 4
      return ('fps', f'{fps/1000}')
    # bad msg
    return ('error', 'BAD')

  def _utils_read_int(self, base):
    return int.from_bytes(self.data[base:base+4], byteorder=sys.byteorder, signed=False)
