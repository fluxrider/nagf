#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

import os
import gi
import contextlib
gi.require_version('Gst', '1.0')
gi.require_version('GstAudio', '1.0')
from gi.repository import Gst, GstAudio

Gst.init(None)
player = Gst.ElementFactory.make("playbin", "player")

def on_gst_bus_message(bus, message):
  if message.type == Gst.MessageType.EOS:
    # TODO auto loop for bg
    print("EOF")
    pass
  elif message.type == Gst.MessageType.ERROR:
    print(f"snd-gstreamer ERROR {message.parse_error()}")
  else:
    print(message)
bus = player.get_bus()
bus.add_signal_watch()
bus.connect("message", on_gst_bus_message)

def gst_start(path):
  gst_stop()
  # TODO how to get a File not found exception? This currently fails silently.
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

# listen to client messages
server_fifo_path = 'snd-gstream.fifo'
with contextlib.suppress(FileNotFoundError): os.remove(server_fifo_path)
os.mkfifo(server_fifo_path)
running = True
while running:
  with open(server_fifo_path, 'r') as f:
    for line in f:
      line = line.strip()
      print(line)
      if line == 'shutdown':
        running = False
        break
      if line.startswith('stream '):
        gst_start(f'file://{os.path.abspath(line[7:])}')
      elif line == 'stop':
        gst_stop()
      elif line.startswith('volume '):
        parts = line.split()
        gst_set_volume(float(parts[1]), float(parts[2]))
os.remove(server_fifo_path)
