#!/usr/bin/env python3
# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
# PYTHONPATH=. LD_LIBRARY_PATH=msglib ./servers/snd-gstreamer.py

# A simple gstreamer-based implementation of a sound server.
# - blindly uses a high-level 'playbin' for the bg song.
# - blindly uses multiple high-level 'playbin' for sound clips.
# - fail fast

import os
import gi
import sys
import errno
import threading
import contextlib
gi.require_version('Gst', '1.0')
gi.require_version('GstAudio', '1.0')
from gi.repository import Gst, GstAudio
from msglib.msgmgr import MsgMgr

should_quit = threading.Event()
error = None
Gst.init(None)

# utils
def combine_volume(cubic, linear):
  return linear * GstAudio.StreamVolume.convert_volume(GstAudio.StreamVolumeFormat.CUBIC, GstAudio.StreamVolumeFormat.LINEAR, cubic)

# procedural audio stream
def on_need_data(src, n, extra):
  global error
  global should_quit
  try:
    client = extra[0]
    data = client.send(n.to_bytes(4, byteorder=sys.byteorder, signed=False))
    if len(data) == 0:
      src.emit('end_of_stream')
    else:
      src.emit('push-buffer', Gst.Buffer.new_wrapped(bytearray(data)))
  except Exception as e:
    error = e
    should_quit.set()

def on_enough_data(src):
  global error
  global should_quit
  try:
    src.emit('end_of_stream')
  except Exception as e:
    error = e
    should_quit.set()

# background song (i.e. only one plays at a time)
bg = Gst.ElementFactory.make('playbin')
bg.set_property('video-sink', Gst.ElementFactory.make('fakesink'))

# listen to gstreamer events
def handle_message_loop(player):
  global error
  global should_quit
  try:
    bus = player.get_bus()
    while True:
      msg = bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.EOS | Gst.MessageType.ERROR)
      # fail fast on error
      if msg.type == Gst.MessageType.ERROR:
        player.set_state(Gst.State.NULL)
        error = msg.parse_error()
        should_quit.set()
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
    should_quit.set()

# listen to client messages (from the FIFO)
server_fifo_path = 'snd-gstream.fifo'
def handle_fifo_loop():
  global error
  global should_quit
  try:
    with contextlib.suppress(FileNotFoundError): os.remove(server_fifo_path)
    os.mkfifo(server_fifo_path)
    while True:
      with open(server_fifo_path, 'r') as f:
        for line in f:
          line = line.strip()
          print(line)
          # cmd: shutdown
          if line == 'shutdown':
            should_quit.set()
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
            p.set_property('video-sink', Gst.ElementFactory.make('fakesink'))
            p.set_property('uri', f'file://{os.path.abspath(path)}')
            p.set_property('volume', combine_volume(cubic, linear))
            p.set_state(Gst.State.PLAYING)
            threading.Thread(target=handle_message_loop, args=(p,), daemon=True).start()
          # cmd: raw <shm-channel> <volume_cubic> <volume_linear>
          elif line.startswith('raw '):
            parts = line.split()
            cubic = float(parts[-2])
            linear = float(parts[-1])
            path = ' '.join(parts[1:-2])
            client = MsgMgr(path)
            source = Gst.ElementFactory.make("appsrc")
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
            volume.set_property('volume', combine_volume(cubic, linear))
            source.set_property('duration', Gst.CLOCK_TIME_NONE)
            source.set_property('size', -1)
            source.set_property('is-live', True)
            source.set_property('blocksize', 8000 / 60)
            source.connect('need-data', on_need_data, (client,))
            source.connect('enough-data', on_enough_data)
            pipe.set_state(Gst.State.PLAYING)
            threading.Thread(target=handle_message_loop, args=(pipe,), daemon=True).start()
          else:
            print(f'snd-gstreamer CMD ERROR {line}')
  except Exception as e:
    error = e
    should_quit.set()

# start listening
bg.set_property('volume', combine_volume(1, 1))
handle_fifo_loop_thread = threading.Thread(target=handle_fifo_loop, daemon=True)
handle_fifo_loop_thread.start()
threading.Thread(target=handle_message_loop, args=(bg,), daemon=True).start()

try:
  should_quit.wait()
finally:
  with contextlib.suppress(FileNotFoundError): os.remove(server_fifo_path)
  if error: print(f'{str(error)}')
