#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.

# A simple gstreamer-based implementation of a sound server.
# - blindly uses a high-level 'playbin' for the bg song.
# - blindly uses multiple high-level 'playbin' for sound clips.
# - fail fast

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

def combine_volume(cubic, linear):
  return linear * GstAudio.StreamVolume.convert_volume(GstAudio.StreamVolumeFormat.CUBIC, GstAudio.StreamVolumeFormat.LINEAR, cubic)

# background song (i.e. only one plays at a time)
bg = Gst.ElementFactory.make('playbin')
# TODO may want to: fakesink = Gst.ElementFactory.make("fakesink"); self.player.set_property("video-sink", fakesink)

# listen to gstreamer events
error = None
def handle_message_loop(player):
  global error
  try:
    bus = player.get_bus()
    while True:
      msg = bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.EOS | Gst.MessageType.ERROR)
      # fail fast on error
      if msg.type == Gst.MessageType.ERROR:
        print(f'snd-gstreamer ERROR {msg.parse_error()}')
        error = True
        player.set_state(Gst.State.NULL)
        break
      # loop background song
      elif msg.type == Gst.MessageType.EOS:
        if player == bg:
          bg.seek_simple(Gst.Format.TIME, Gst.SeekFlags.FLUSH | Gst.SeekFlags.KEY_UNIT, 0)
        else:
          player.set_state(Gst.State.NULL)
          break
  except Exception as e:
    error = e

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
          bg.set_state(Gst.State.NULL)
          bg.set_property('uri', f'file://{os.path.abspath(line[7:])}')
          bg.set_state(Gst.State.PLAYING)
        # cmd: stop (the stream)
        elif line == 'stop':
          bg.set_state(Gst.State.NULL)
        # cmd: volume <cubic> <linear>
        elif line.startswith('volume '):
          parts = line.split()
          bg.set_property('volume', combine_volume(float(parts[-2]), float(parts[-1])))
        # cmd: cache <path>
        elif line.startswith('cache '):
          # ignore TODO still track coherence of cache/fire/zap
          pass
        # cmd: zap <path>
        elif line.startswith('zap '):
          # ignore
          pass
        # cmd: fire <path> <volume_cubic> <volume_linear>
        elif line.startswith('fire '):
          parts = line.split()
          cubic = float(parts[-2])
          linear = float(parts[-1])
          path = ' '.join(parts[1:-2])
          p = Gst.ElementFactory.make('playbin')
          p.set_property('uri', f'file://{os.path.abspath(path)}')
          p.set_property('volume', combine_volume(cubic, linear))
          p.set_state(Gst.State.PLAYING)
          threading.Thread(target=handle_message_loop, args=(p,), daemon=True).start()
        # cmd: raw <fifo-path>
        elif line.startswith('raw '):
          parts = line.split()
          cubic = float(parts[-2])
          linear = float(parts[-1])
          path = ' '.join(parts[1:-2])
          source = Gst.ElementFactory.make("filesrc")
          raw = Gst.ElementFactory.make('rawaudioparse')
          raw.set_property('format', 'pcm')
          raw.set_property('pcm-format', 's8')
          raw.set_property('sample-rate', 8000)
          raw.set_property('num-channels', 1)
          raw.set_property('use-sink-caps', False)
          convert = Gst.ElementFactory.make("audioconvert")
          resample = Gst.ElementFactory.make("audioresample")
          volume = Gst.ElementFactory.make("volume")
          sink = Gst.ElementFactory.make("autoaudiosink")
          pipe = Gst.Pipeline.new()
          pipe.add(source)
          pipe.add(raw)
          pipe.add(convert)
          pipe.add(resample)
          pipe.add(volume)
          pipe.add(sink)
          source.link(raw)
          raw.link(convert)
          convert.link(resample)
          resample.link(volume)
          volume.link(sink)
          source.set_property("location", os.path.abspath(path))
          volume.set_property('volume', combine_volume(cubic, linear))
          pipe.set_state(Gst.State.PLAYING)
          threading.Thread(target=handle_message_loop, args=(pipe,), daemon=True).start()
        else:
          print(f'snd-gstreamer CMD ERROR {line}')

# start listening
bg.set_property('volume', combine_volume(1, 1))
handle_fifo_loop_thread = threading.Thread(target=handle_fifo_loop, daemon=True)
handle_fifo_loop_thread.start()
threading.Thread(target=handle_message_loop, args=(bg,), daemon=True).start()

try:
  # TODO find a way to do this check without an ugly delay (and intense checks)
  while handle_fifo_loop_thread.is_alive() and not error:
    time.sleep(2)
finally:
  with contextlib.suppress(FileNotFoundError): os.remove(server_fifo_path)
  print(f'{str(error)}')
