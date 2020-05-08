#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

import os
import gi
import sys
import time
import errno
import threading
import contextlib
gi.require_version('Gst', '1.0')
gi.require_version('GstAudio', '1.0')
from gi.repository import Gst, GstAudio

Gst.init(None)
player = Gst.ElementFactory.make("playbin", "player")

# streamed song utils (e.g. bg song)
def gst_start(path):
  gst_stop()
  player.set_property("uri", path)
  player.set_state(Gst.State.PLAYING)

def gst_toggle_pause():
  reply = player.get_state(Gst.CLOCK_TIME_NONE)
  if reply[0]:
    if reply[1] == Gst.State.PLAYING:
      player.set_state(Gst.State.PAUSED)
    elif reply[1] == Gst.State.PAUSED:
      player.set_state(Gst.State.PLAYING)

def gst_stop():
  player.set_state(Gst.State.NULL)

def gst_seek(ns):
  player.seek_simple(Gst.Format.TIME, Gst.SeekFlags.FLUSH | Gst.SeekFlags.KEY_UNIT, ns)

# volume is always combo of two parameters (cubic and linear), I detest the one slider paradigm
def gst_set_volume(cubic, linear):
  player.set_property("volume", linear * GstAudio.StreamVolume.convert_volume(GstAudio.StreamVolumeFormat.CUBIC, GstAudio.StreamVolumeFormat.LINEAR, cubic))

# TODO status debug spew request (e.g. show currently playing bg song, position, duration, volumes)
#player.query_duration(Gst.Format.TIME)
#reply[1] / Gst.SECOND

# TODO stack of bg song, which takes care of resume

# TODO non-stream audio clip

# TODO procedural sound clip

# listen to gstreamer events
def handle_message_loop():
  bus = player.get_bus()
  while True:
    msg = bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.EOS | Gst.MessageType.ERROR)
    if msg.type == Gst.MessageType.ERROR:
      print(f'snd-gstreamer ERROR {msg.parse_error()}')
    elif msg.type == Gst.MessageType.EOS:
      print('snd-gstreamer EOS')

# listen to client messages (from the FIFO)
server_fifo_path = 'snd-gstream.fifo'
def handle_fifo_loop():
  with contextlib.suppress(FileNotFoundError): os.remove(server_fifo_path)
  os.mkfifo(server_fifo_path)
  while True:
    with open(server_fifo_path, 'r') as f:
      for line in f:
        line = line.strip()
        print(line)
        # cmd: shutdown
        if line == 'shutdown':
          return
        # cmd: stream <path>
        if line.startswith('stream '):
          gst_start(f'file://{os.path.abspath(line[7:])}')
        # cmd: stop (the stream)
        elif line == 'stop':
          gst_stop()
        # cmd: volume <cubic> <linear> (normalized [0-1] of course)
        elif line.startswith('volume '):
          parts = line.split()
          gst_set_volume(float(parts[1]), float(parts[2]))
        else:
          print(f'snd-gstreamer CMD ERROR {line}')

# start listening
handle_fifo_loop_thread = threading.Thread(target=handle_fifo_loop, daemon=True)
handle_fifo_loop_thread.start()
handle_message_loop_thread = threading.Thread(target=handle_message_loop, daemon=True)
handle_message_loop_thread.start()

try:
  # TODO I want to move on as soon as one of my thread exits. I have not found a way to do this without an ugly delay.
  while handle_fifo_loop_thread.is_alive() and handle_message_loop_thread.is_alive():
    time.sleep(2)
finally:
  with contextlib.suppress(FileNotFoundError): os.remove(server_fifo_path)

