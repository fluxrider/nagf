#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

import time
import math
import bitarray
from msglib.msgmgr import MsgMgr

A = 0
B = 1
C = 2
D = 3
E = 4
F = 5
G = 6
H = 7
I = 8
J = 9
K = 10
L = 11
M = 12
N = 13
O = 14
P = 15
Q = 16
R = 17
S = 18
T = 19
U = 20
V = 21
W = 22
X = 23
Y = 24
Z = 25
N0 = 26
N1 = 27
N2 = 28
N3 = 29
N4 = 30
N5 = 31
N6 = 32
N7 = 33
N8 = 34
N9 = 35
UP = 36
DOWN = 37
LEFT = 38
RIGHT = 39
ESC = 40
TAB = 41
ALT_L = 42
ALT_R = 43
CTRL_L = 44
CTRL_R = 45
SHIFT_L = 46
SHIFT_R = 47
SPACE = 48
F1 = 49
F2 = 50
F3 = 51
F4 = 52
F5 = 53
F6 = 54
F7 = 55
F8 = 56
F9 = 57
F10 = 58
F11 = 59
F12 = 60
ENTER = 61
BACKSPACE = 62
PGUP = 63
PGDOWN = 64
GRAVE = 65
BRACE_L = 66
BRACE_R = 67
DOT = 68
SEMICOLON = 69
APOSTROPHE = 70
BACKSLASH = 71
SLASH = 72
COMMA = 73
M0_L = 74
M0_R = 75
M0_M = 76
M1_L = 77
M1_R = 78
M1_M = 79
M2_L = 80
M2_R = 81
M2_M = 82
M3_L = 83
M3_R = 84
M3_M = 85
G0_R1 = 86
G0_R2 = 87
G0_R3 = 88
G0_L1 = 89
G0_L2 = 90
G0_L3 = 91
G0_START = 92
G0_HOME = 93
G0_SELECT = 94
G0_NORTH = 95
G0_SOUTH = 96
G0_EAST = 97
G0_WEST = 98
G0_UP = 99
G0_DOWN = 100
G0_LEFT = 101
G0_RIGHT = 102
G1_R1 = 103
G1_R2 = 104
G1_R3 = 105
G1_L1 = 106
G1_L2 = 107
G1_L3 = 108
G1_START = 109
G1_HOME = 110
G1_SELECT = 111
G1_NORTH = 112
G1_SOUTH = 113
G1_EAST = 114
G1_WEST = 115
G1_UP = 116
G1_DOWN = 117
G1_LEFT = 118
G1_RIGHT = 119
G2_R1 = 120
G2_R2 = 121
G2_R3 = 122
G2_L1 = 123
G2_L2 = 124
G2_L3 = 125
G2_START = 126
G2_HOME = 127
G2_SELECT = 128
G2_NORTH = 129
G2_SOUTH = 130
G2_EAST = 131
G2_WEST = 132
G2_UP = 133
G2_DOWN = 134
G2_LEFT = 135
G2_RIGHT = 136
G3_R1 = 137
G3_R2 = 138
G3_R3 = 139
G3_L1 = 140
G3_L2 = 141
G3_L3 = 142
G3_START = 143
G3_HOME = 144
G3_SELECT = 145
G3_NORTH = 146
G3_SOUTH = 147
G3_EAST = 148
G3_WEST = 149
G3_UP = 150
G3_DOWN = 151
G3_LEFT = 152
G3_RIGHT = 153
K_COUNT = 154
KEY_NONE = 255
M_COUNT = 4
G_COUNT = 4
H_COUNT = 16

def u8_to_s8(u8):
  if u8 > 127: return u8 - 255 - 1
  return u8

class Evt:
  def __init__(self, name):
    self.client = MsgMgr(name)
    self.data = None

  def poll(self, command=''):
    self.data = self.client.send(command.encode())

  def held(self, key):
    return ((self._get_half_byte(key) >> 3) & 1) != 0

  def pressed(self, key):
    return self.pressed_count(key) > 0

  def pressed_count(self, key):
    return ((self._get_half_byte(key) >> 1) & 3)

  def released(self, key):
    return (self._get_half_byte(key) & 1) != 0

  def mouse(self, i=0):
    if i < 0 or i >= M_COUNT: raise RuntimeException(f'Bad mouse index ({i}). Must be between 0 and {M_COUNT-1}.')
    index = math.ceil(K_COUNT / 2) + i * 3 # where 3 is (mx, my, mw)
    return (u8_to_s8(self.data[index]), u8_to_s8(self.data[index+1]), u8_to_s8(self.data[index+2]))

  def axis_and_triggers(self, i=0):
    if i < 0 or i >= G_COUNT: raise RuntimeException(f'Bad virtual gamepad index ({i}). Must be between 0 and {G_COUNT-1}.')
    index = math.ceil(K_COUNT / 2) + M_COUNT * 3 + i * 6 # where 3 is (mx, my, mw) and 6 is (lx, lr, rx, ry, lt, rt)
    return (u8_to_s8(self.data[index]), u8_to_s8(self.data[index+1]), u8_to_s8(self.data[index+2]), u8_to_s8(self.data[index+3]), self.data[index+4], self.data[index+5])
    
  def histokey(self):
    index = math.ceil(K_COUNT / 2) + M_COUNT * 3 + G_COUNT * 6 # where 3 is (mx, my, mw) and 6 is (lx, lr, rx, ry, lt, rt)
    b = bitarray.bitarray()
    b.frombytes(bytes([self.data[index + H_COUNT], self.data[index + H_COUNT + 1]]))
    h = []
    for i in range(H_COUNT):
      k = self.data[index + i]
      if k != KEY_NONE: h.append((k, b[i]))
    return h

  # TODO convenient mx,my,mw
  # TODO convenient axis/trigger with deadzone normalize
  
  # context managers interface
  def __enter__(self):
    return self

  def __exit__(self, type, value, tb):
    self.client.disconnect()

  # utils
  def _get_half_byte(self, key):
    if key < 0 or key >= K_COUNT: raise RuntimeException(f'Unknown key ({key}).')
    v = self.data[int(key / 2)]
    if key % 2 == 0: v = v >> 4
    return v
