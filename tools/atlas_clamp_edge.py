# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later.
# python -B atlas_clamp_edge.py in.png out.png 16

# This script adds a 1 pixel clamp_edge margin to every tiles in the atlas
import os
import sys
from PIL import Image
#import numpy

input_file = sys.argv[1]
output_file = sys.argv[2]
ts = int(sys.argv[3])

in_image = Image.open(input_file)
w, h = in_image.size

C = int(w / ts)
R = int(h / ts)
W = C * (ts + 2)
H = R * (ts + 2)
out_image = Image.new(in_image.mode, (W, H))

for r in range(R):
  for c in range(C):
    # copy tile, and use edges as margin
    x = ts * c
    y = ts * r
    tile = in_image.crop((x, y, x + ts, y + ts))
    left = in_image.crop((x, y, x + 1, y + ts))
    right = in_image.crop((x + ts - 1, y, x + ts, y + ts))
    top = in_image.crop((x, y, x + ts, y + 1))
    bottom = in_image.crop((x, y + ts - 1, x + ts, y + ts))
    x = 1 + (ts + 2) * c
    y = 1 + (ts + 2) * r
    out_image.paste(tile, (x, y, x + ts, y + ts))
    out_image.paste(left, (x - 1, y, x, y + ts))
    out_image.paste(right, (x + ts, y, x + ts + 1, y + ts))
    out_image.paste(top, (x, y - 1, x + ts, y))
    out_image.paste(bottom, (x, y + ts, x + ts, y + ts + 1))
    # corner values
    out_image.putpixel((x-1,y-1), out_image.getpixel((x, y)))
    out_image.putpixel((x-1,y+ts), out_image.getpixel((x, y+ts-1)))
    out_image.putpixel((x+ts,y-1), out_image.getpixel((x+ts-1, y)))
    out_image.putpixel((x+ts,y+ts), out_image.getpixel((x+ts-1, y+ts-1)))

#data = numpy.zeros((H, W, 4), dtype=numpy.uint8)
#data[0:256, 0:256] = [255, 0, 0] # red patch in upper left
#image = Image.fromarray(data, 'RGBA')

out_image.save(output_file)
