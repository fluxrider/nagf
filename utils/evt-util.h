#pragma once
// Copyright 2020 David Lareau. This source code form is subject to the terms of the Mozilla Public License 2.0.

#include <stdint.h>
#include <stdbool.h>
#include "srr.h"

enum {
  K_A = 0,
  K_B = 1,
  K_C = 2,
  K_D = 3,
  K_E = 4,
  K_F = 5,
  K_G = 6,
  K_H = 7,
  K_I = 8,
  K_J = 9,
  K_K = 10,
  K_L = 11,
  K_M = 12,
  K_N = 13,
  K_O = 14,
  K_P = 15,
  K_Q = 16,
  K_R = 17,
  K_S = 18,
  K_T = 19,
  K_U = 20,
  K_V = 21,
  K_W = 22,
  K_X = 23,
  K_Y = 24,
  K_Z = 25,
  K_N0 = 26,
  K_N1 = 27,
  K_N2 = 28,
  K_N3 = 29,
  K_N4 = 30,
  K_N5 = 31,
  K_N6 = 32,
  K_N7 = 33,
  K_N8 = 34,
  K_N9 = 35,
  K_UP = 36,
  K_DOWN = 37,
  K_LEFT = 38,
  K_RIGHT = 39,
  K_ESC = 40,
  K_TAB = 41,
  K_ALT_L = 42,
  K_ALT_R = 43,
  K_CTRL_L = 44,
  K_CTRL_R = 45,
  K_SHIFT_L = 46,
  K_SHIFT_R = 47,
  K_SPACE = 48,
  K_F1 = 49,
  K_F2 = 50,
  K_F3 = 51,
  K_F4 = 52,
  K_F5 = 53,
  K_F6 = 54,
  K_F7 = 55,
  K_F8 = 56,
  K_F9 = 57,
  K_F10 = 58,
  K_F11 = 59,
  K_F12 = 60,
  K_ENTER = 61,
  K_BACKSPACE = 62,
  K_PGUP = 63,
  K_PGDOWN = 64,
  K_GRAVE = 65,
  K_BRACE_L = 66,
  K_BRACE_R = 67,
  K_DOT = 68,
  K_SEMICOLON = 69,
  K_APOSTROPHE = 70,
  K_BACKSLASH = 71,
  K_SLASH = 72,
  K_COMMA = 73,
  M0_L = 74,
  M0_R = 75,
  M0_M = 76,
  M1_L = 77,
  M1_R = 78,
  M1_M = 79,
  M2_L = 80,
  M2_R = 81,
  M2_M = 82,
  M3_L = 83,
  M3_R = 84,
  M3_M = 85,
  G0_R1 = 86,
  G0_R2 = 87,
  G0_R3 = 88,
  G0_L1 = 89,
  G0_L2 = 90,
  G0_L3 = 91,
  G0_START = 92,
  G0_HOME = 93,
  G0_SELECT = 94,
  G0_NORTH = 95,
  G0_SOUTH = 96,
  G0_EAST = 97,
  G0_WEST = 98,
  G0_UP = 99,
  G0_DOWN = 100,
  G0_LEFT = 101,
  G0_RIGHT = 102,
  G1_R1 = 103,
  G1_R2 = 104,
  G1_R3 = 105,
  G1_L1 = 106,
  G1_L2 = 107,
  G1_L3 = 108,
  G1_START = 109,
  G1_HOME = 110,
  G1_SELECT = 111,
  G1_NORTH = 112,
  G1_SOUTH = 113,
  G1_EAST = 114,
  G1_WEST = 115,
  G1_UP = 116,
  G1_DOWN = 117,
  G1_LEFT = 118,
  G1_RIGHT = 119,
  G2_R1 = 120,
  G2_R2 = 121,
  G2_R3 = 122,
  G2_L1 = 123,
  G2_L2 = 124,
  G2_L3 = 125,
  G2_START = 126,
  G2_HOME = 127,
  G2_SELECT = 128,
  G2_NORTH = 129,
  G2_SOUTH = 130,
  G2_EAST = 131,
  G2_WEST = 132,
  G2_UP = 133,
  G2_DOWN = 134,
  G2_LEFT = 135,
  G2_RIGHT = 136,
  G3_R1 = 137,
  G3_R2 = 138,
  G3_R3 = 139,
  G3_L1 = 140,
  G3_L2 = 141,
  G3_L3 = 142,
  G3_START = 143,
  G3_HOME = 144,
  G3_SELECT = 145,
  G3_NORTH = 146,
  G3_SOUTH = 147,
  G3_EAST = 148,
  G3_WEST = 149,
  G3_UP = 150,
  G3_DOWN = 151,
  G3_LEFT = 152,
  G3_RIGHT = 153,
  K_COUNT = 154,
  KEY_NONE = 255
};

struct evt_mouse {
  int8_t dx;
  int8_t dy;
  int8_t dwheel;
};

struct evt_axis_and_triggers_raw {
  int8_t lx;
  int8_t ly;
  int8_t rx;
  int8_t ry;
  int8_t lt;
  int8_t rt;
};

struct evt_axis_and_triggers_normalized {
  double lx;
  double ly;
  double rx;
  double ry;
  double lt;
  double rt;
};

bool evt_held(struct srr * evt, int key);
bool evt_pressed(struct srr * evt, int key);
int evt_pressed_count(struct srr * evt, int key);
int evt_released(struct srr * evt, int key);
struct evt_mouse * evt_mouse(struct srr * evt, int i);
struct evt_axis_and_triggers_raw * evt_axis_and_triggers(struct srr * evt, int i);
int * evt_histokey(struct srr * evt, int pressed[16]);
struct evt_axis_and_triggers_normalized evt_deadzoned(struct evt_axis_and_triggers_raw * a, double axis_deadzone, double trigger_deadzone);
