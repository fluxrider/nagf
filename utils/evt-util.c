// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "srr.h"
#include "evt-util.h"

#define M_COUNT 4
#define G_COUNT 4
#define H_COUNT 16

static int _get_half_byte(struct srr_direct * mem, int key) {
  if(key < 0 || key >= K_COUNT) { fprintf(stderr, "Unknown key (%d).", key); return 0; }
  int v = mem->msg[key / 2];
  if(key % 2 == 0) v = v >> 4;
  return v;
}

bool evt_held(struct srr * evt, int key) {
  return ((_get_half_byte(srr_direct(evt), key) >> 3) & 1) != 0;
}

bool evt_pressed(struct srr * evt, int key) {
  return evt_pressed_count(evt, key) > 0;
}

int evt_pressed_count(struct srr * evt, int key) {
  return (_get_half_byte(srr_direct(evt), key) >> 1) & 3;
}

int evt_released(struct srr * evt, int key) {
  return (_get_half_byte(srr_direct(evt), key) & 1) != 0;
}

struct evt_mouse * evt_mouse(struct srr * evt, int i) {
  if(i < 0 || i >= M_COUNT) { fprintf(stderr, "Bad mouse index (%d). Must be between 0 and %d.", i, M_COUNT-1); return NULL; }
  int index = (K_COUNT + (2-1) / 2) + i * 3; // where 3 is (mx, my, mw)
  return (struct evt_mouse *)&srr_direct(evt)->msg[index];
}


struct evt_axis_and_triggers_raw * evt_axis_and_triggers(struct srr * evt, int i) {
  if(i < 0 || i >= G_COUNT) { fprintf(stderr, "Bad virtual gamepad index (%d). Must be between 0 and %d.", i, M_COUNT-1); return NULL; }
  int index = (K_COUNT + (2-1) / 2) + M_COUNT * 3 + i * 6; // where 3 is (mx, my, mw) and 6 is (lx, lr, rx, ry, lt, rt)
  return (struct evt_axis_and_triggers_raw *)&srr_direct(evt)->msg[index];
}

int * evt_histokey(struct srr * evt, int pressed[16]) {
  int index = (K_COUNT + (2-1) / 2) + M_COUNT * 3 + G_COUNT * 6; // where 3 is (mx, my, mw) and 6 is (lx, lr, rx, ry, lt, rt)
  struct srr_direct * mem = srr_direct(evt);
  uint16_t bits = *(uint16_t *)&mem->msg[index + H_COUNT];
  for(int i = 0; i < 16; i++) pressed[i] = ((bits >> (15 - i)) & 1) != 0;
  return (int *)&mem->msg[index];
}

static double _deadzone_util(int8_t v, int d) {
  int flip = (v > 0)? 1 : -1;
  return (abs(v) < abs(d))? 0 : (v - d * flip) / (double)(127 - abs(d));
}

struct evt_axis_and_triggers_normalized evt_deadzoned(struct evt_axis_and_triggers_raw * a, double axis_deadzone, double trigger_deadzone) {
  struct evt_axis_and_triggers_normalized dzoned;
  int ad = (int)(127 * axis_deadzone);
  int td = (int)(255 * trigger_deadzone);
  dzoned.lx = _deadzone_util(a->lx, ad);
  dzoned.ly = _deadzone_util(a->ly, ad);
  dzoned.rx = _deadzone_util(a->rx, ad);
  dzoned.ry = _deadzone_util(a->ry, ad);
  dzoned.lt = (a->lt < td)? 0 : (a->lt - td) / (double)(255 - td);
  dzoned.rt = (a->rt < td)? 0 : (a->rt - td) / (double)(255 - td);
  return dzoned;
}
