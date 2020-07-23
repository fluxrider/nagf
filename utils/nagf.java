// Copyright 2020 David Lareau. This source code form is subject to the terms of the Mozilla Public License 2.0.

import java.nio.*;

public class nagf {

  public static final int GFX_STAT_ERR = 0;
  public static final int GFX_STAT_IMG = 1;
  public static final int GFX_STAT_FNT = 2;
  public static final int GFX_STAT_DLT = 3;
  public static final int GFX_STAT_ALL = 4;
  public static final int GFX_STAT_COUNT = 5;

  public static final int K_A = 0;
  public static final int K_B = 1;
  public static final int K_C = 2;
  public static final int K_D = 3;
  public static final int K_E = 4;
  public static final int K_F = 5;
  public static final int K_G = 6;
  public static final int K_H = 7;
  public static final int K_I = 8;
  public static final int K_J = 9;
  public static final int K_K = 10;
  public static final int K_L = 11;
  public static final int K_M = 12;
  public static final int K_N = 13;
  public static final int K_O = 14;
  public static final int K_P = 15;
  public static final int K_Q = 16;
  public static final int K_R = 17;
  public static final int K_S = 18;
  public static final int K_T = 19;
  public static final int K_U = 20;
  public static final int K_V = 21;
  public static final int K_W = 22;
  public static final int K_X = 23;
  public static final int K_Y = 24;
  public static final int K_Z = 25;
  public static final int K_N0 = 26;
  public static final int K_N1 = 27;
  public static final int K_N2 = 28;
  public static final int K_N3 = 29;
  public static final int K_N4 = 30;
  public static final int K_N5 = 31;
  public static final int K_N6 = 32;
  public static final int K_N7 = 33;
  public static final int K_N8 = 34;
  public static final int K_N9 = 35;
  public static final int K_UP = 36;
  public static final int K_DOWN = 37;
  public static final int K_LEFT = 38;
  public static final int K_RIGHT = 39;
  public static final int K_ESC = 40;
  public static final int K_TAB = 41;
  public static final int K_ALT_L = 42;
  public static final int K_ALT_R = 43;
  public static final int K_CTRL_L = 44;
  public static final int K_CTRL_R = 45;
  public static final int K_SHIFT_L = 46;
  public static final int K_SHIFT_R = 47;
  public static final int K_SPACE = 48;
  public static final int K_F1 = 49;
  public static final int K_F2 = 50;
  public static final int K_F3 = 51;
  public static final int K_F4 = 52;
  public static final int K_F5 = 53;
  public static final int K_F6 = 54;
  public static final int K_F7 = 55;
  public static final int K_F8 = 56;
  public static final int K_F9 = 57;
  public static final int K_F10 = 58;
  public static final int K_F11 = 59;
  public static final int K_F12 = 60;
  public static final int K_ENTER = 61;
  public static final int K_BACKSPACE = 62;
  public static final int K_PGUP = 63;
  public static final int K_PGDOWN = 64;
  public static final int K_GRAVE = 65;
  public static final int K_BRACE_L = 66;
  public static final int K_BRACE_R = 67;
  public static final int K_DOT = 68;
  public static final int K_SEMICOLON = 69;
  public static final int K_APOSTROPHE = 70;
  public static final int K_BACKSLASH = 71;
  public static final int K_SLASH = 72;
  public static final int K_COMMA = 73;
  public static final int M0_L = 74;
  public static final int M0_R = 75;
  public static final int M0_M = 76;
  public static final int M1_L = 77;
  public static final int M1_R = 78;
  public static final int M1_M = 79;
  public static final int M2_L = 80;
  public static final int M2_R = 81;
  public static final int M2_M = 82;
  public static final int M3_L = 83;
  public static final int M3_R = 84;
  public static final int M3_M = 85;
  public static final int G0_R1 = 86;
  public static final int G0_R2 = 87;
  public static final int G0_R3 = 88;
  public static final int G0_L1 = 89;
  public static final int G0_L2 = 90;
  public static final int G0_L3 = 91;
  public static final int G0_START = 92;
  public static final int G0_HOME = 93;
  public static final int G0_SELECT = 94;
  public static final int G0_NORTH = 95;
  public static final int G0_SOUTH = 96;
  public static final int G0_EAST = 97;
  public static final int G0_WEST = 98;
  public static final int G0_UP = 99;
  public static final int G0_DOWN = 100;
  public static final int G0_LEFT = 101;
  public static final int G0_RIGHT = 102;
  public static final int G1_R1 = 103;
  public static final int G1_R2 = 104;
  public static final int G1_R3 = 105;
  public static final int G1_L1 = 106;
  public static final int G1_L2 = 107;
  public static final int G1_L3 = 108;
  public static final int G1_START = 109;
  public static final int G1_HOME = 110;
  public static final int G1_SELECT = 111;
  public static final int G1_NORTH = 112;
  public static final int G1_SOUTH = 113;
  public static final int G1_EAST = 114;
  public static final int G1_WEST = 115;
  public static final int G1_UP = 116;
  public static final int G1_DOWN = 117;
  public static final int G1_LEFT = 118;
  public static final int G1_RIGHT = 119;
  public static final int G2_R1 = 120;
  public static final int G2_R2 = 121;
  public static final int G2_R3 = 122;
  public static final int G2_L1 = 123;
  public static final int G2_L2 = 124;
  public static final int G2_L3 = 125;
  public static final int G2_START = 126;
  public static final int G2_HOME = 127;
  public static final int G2_SELECT = 128;
  public static final int G2_NORTH = 129;
  public static final int G2_SOUTH = 130;
  public static final int G2_EAST = 131;
  public static final int G2_WEST = 132;
  public static final int G2_UP = 133;
  public static final int G2_DOWN = 134;
  public static final int G2_LEFT = 135;
  public static final int G2_RIGHT = 136;
  public static final int G3_R1 = 137;
  public static final int G3_R2 = 138;
  public static final int G3_R3 = 139;
  public static final int G3_L1 = 140;
  public static final int G3_L2 = 141;
  public static final int G3_L3 = 142;
  public static final int G3_START = 143;
  public static final int G3_HOME = 144;
  public static final int G3_SELECT = 145;
  public static final int G3_NORTH = 146;
  public static final int G3_SOUTH = 147;
  public static final int G3_EAST = 148;
  public static final int G3_WEST = 149;
  public static final int G3_UP = 150;
  public static final int G3_DOWN = 151;
  public static final int G3_LEFT = 152;
  public static final int G3_RIGHT = 153;
  public static final int K_COUNT = 154;
  public static final int KEY_NONE = 255;

  public static final int M_COUNT = 4;
  public static final int G_COUNT = 4;
  public static final int H_COUNT = 16;

  private static int _get_half_byte(ByteBuffer msg, int key) {
    if(key < 0 || key >= K_COUNT) { System.err.printf("Unknown key (%d).", key); return 0; }
    int v = msg.get(key / 2);
    if(key % 2 == 0) v = v >>> 4;
    return v;
  }

  public static boolean evt_held(ByteBuffer msg, int key) {
    return ((_get_half_byte(msg, key) >>> 3) & 1) != 0;
  }

  public static boolean evt_pressed(ByteBuffer msg, int key) {
    return evt_pressed_count(msg, key) > 0;
  }

  public static int evt_pressed_count(ByteBuffer msg, int key) {
    return (_get_half_byte(msg, key) >>> 1) & 3;
  }

  public static boolean evt_released(ByteBuffer msg, int key) {
    return (_get_half_byte(msg, key) & 1) != 0;
  }

  public static class evt_mouse {
    byte dx;
    byte dy;
    byte dwheel;
  }

  public static class evt_axis_and_triggers_raw {
    byte lx;
    byte ly;
    byte rx;
    byte ry;
    byte lt;
    byte rt;
  }

  public static class evt_axis_and_triggers_normalized {
    double lx;
    double ly;
    double rx;
    double ry;
    double lt;
    double rt;
  }

  public static evt_mouse evt_mouse(ByteBuffer msg, int i) {
    if(i < 0 || i >= M_COUNT) { System.err.printf("Bad mouse index (%d). Must be between 0 and %d.", i, M_COUNT-1); return null; }
    int index = ((K_COUNT + (2-1)) / 2) + i * 3; // where 3 is (mx, my, mw)
    evt_mouse evt = new evt_mouse();
    evt.dx = msg.get(index);
    evt.dy = msg.get(index+1);
    evt.dwheel = msg.get(index+2);
    return evt;
  }


  public static evt_axis_and_triggers_raw evt_axis_and_triggers(ByteBuffer msg, int i) {
    if(i < 0 || i >= G_COUNT) { System.err.printf("Bad virtual gamepad index (%d). Must be between 0 and %d.", i, M_COUNT-1); return null; }
    int index = ((K_COUNT + (2-1)) / 2) + M_COUNT * 3 + i * 6; // where 3 is (mx, my, mw) and 6 is (lx, ly, rx, ry, lt, rt)
    evt_axis_and_triggers_raw evt = new evt_axis_and_triggers_raw();
    evt.lx = msg.get(index);
    evt.ly = msg.get(index+1);
    evt.rx = msg.get(index+2);
    evt.ry = msg.get(index+3);
    evt.lt = msg.get(index+4);
    evt.rt = msg.get(index+5);
    return evt;
  }

  public static void evt_histokey(ByteBuffer msg, int histo[], boolean pressed[]) {
    int index = ((K_COUNT + (2-1)) / 2) + M_COUNT * 3 + G_COUNT * 6; // where 3 is (mx, my, mw) and 6 is (lx, ly, rx, ry, lt, rt)
    int bits = msg.getShort(index + H_COUNT) & 0xFFFF;
    for(int i = 0; i < 16; i++) pressed[i] = ((bits >>> (15 - i)) & 1) != 0;
    for(int i = 0; i < 16; i++) histo[i] = msg.get(index + i);
  }

  private static double _deadzone_util(byte v, int d) {
    int flip = (v > 0)? 1 : -1;
    return (Math.abs(v) < Math.abs(d))? 0 : (v - d * flip) / (double)(127 - Math.abs(d));
  }

  public static evt_axis_and_triggers_normalized evt_deadzoned(evt_axis_and_triggers_raw a, double axis_deadzone, double trigger_deadzone) {
    evt_axis_and_triggers_normalized dzoned = new evt_axis_and_triggers_normalized();
    int ad = (int)(127 * axis_deadzone);
    int td = (int)(255 * trigger_deadzone);
    dzoned.lx = _deadzone_util(a.lx, ad);
    dzoned.ly = _deadzone_util(a.ly, ad);
    dzoned.rx = _deadzone_util(a.rx, ad);
    dzoned.ry = _deadzone_util(a.ry, ad);
    dzoned.lt = (a.lt < td)? 0 : (a.lt - td) / (double)(255 - td);
    dzoned.rt = (a.rt < td)? 0 : (a.rt - td) / (double)(255 - td);
    return dzoned;
  }

}
