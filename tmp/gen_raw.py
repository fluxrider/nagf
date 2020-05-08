#!/usr/bin/env python3
samples_per_second = 8000
hz = 200
n = int((samples_per_second / hz) / 2)

# square wave
data = []
for i in range(0, n):
  data.append(127)
for i in range(0, n):
  data.append(129) # -127

with open('out.raw', 'wb') as f:
  f.write(bytearray(data))
  
with open('out_long.raw', 'wb') as f:
  for i in range(1000):
    f.write(bytearray(data))
    
# gst-launch-1.0 filesrc location=out_long.raw ! rawaudioparse use-sink-caps=false format=pcm pcm-format=s8 sample-rate=8000 num-channels=1 ! audioconvert ! audioresample ! autoaudiosink
