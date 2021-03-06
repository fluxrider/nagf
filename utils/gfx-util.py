# Copyright 2020 David Lareau. This source code form is subject to the terms of the Mozilla Public License 2.0.

import sys

class GfxReply:
  def __init__(self, throw):
    self.data = None
    self.throw = throw

  def set(self, data):
    self.data = data
    self.stat_p = 10

  def focused(self):
    return self.data[0] != 0

  def closing(self):
    return self.data[1] != 0

  def width(self):
    return self._utils_read_int(2)

  def height(self):
    return self._utils_read_int(6)

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
    # delta_time
    if t == 3:
      delta = self._utils_read_int(self.stat_p)
      self.stat_p += 4
      return ('delta', delta / 1000.0)
    # bad msg
    return ('error', 'BAD')

  def _utils_read_int(self, base):
    return int.from_bytes(self.data[base:base+4], byteorder=sys.byteorder, signed=False)
