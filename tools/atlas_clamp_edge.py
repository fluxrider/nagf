# Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later.
# python -B atlas_clamp_edge.py in.png out.png 16 16

# This script adds a 1 pixel clamp_edge margin to every tiles in the atlas, except the actual edges
import os
import sys
from PIL import Image
#import numpy

input_file = sys.argv[1]
output_file = sys.argv[2]
tw = int(sys.argv[3])
th = int(sys.argv[4])

in_image = Image.open(input_file)
w, h = in_image.size

# first do it for all tiles

C = int(w / tw)
R = int(h / th)
W = C * (tw + 2)
H = R * (th + 2)
out_image = Image.new(in_image.mode, (W, H))

for r in range(R):
  for c in range(C):
    # copy tile, and use edges as margin
    x = tw * c
    y = th * r
    tile = in_image.crop((x, y, x + tw, y + th))
    left = in_image.crop((x, y, x + 1, y + th))
    right = in_image.crop((x + tw - 1, y, x + tw, y + th))
    top = in_image.crop((x, y, x + tw, y + 1))
    bottom = in_image.crop((x, y + th - 1, x + tw, y + th))
    x = 1 + (tw + 2) * c
    y = 1 + (th + 2) * r
    out_image.paste(tile, (x, y, x + tw, y + th))
    out_image.paste(left, (x - 1, y, x, y + th))
    out_image.paste(right, (x + tw, y, x + tw + 1, y + th))
    out_image.paste(top, (x, y - 1, x + tw, y))
    out_image.paste(bottom, (x, y + th, x + tw, y + th + 1))
    # corner values
    out_image.putpixel((x-1,y-1), out_image.getpixel((x, y)))
    out_image.putpixel((x-1,y+th), out_image.getpixel((x, y+th-1)))
    out_image.putpixel((x+tw,y-1), out_image.getpixel((x+tw-1, y)))
    out_image.putpixel((x+tw,y+th), out_image.getpixel((x+tw-1, y+th-1)))

# crop out the superflous edges
good = out_image.crop((1,1,W-1, H-1))
out_image = Image.new(in_image.mode, (W-2, H-2))
out_image.paste(good, (0, 0, W-2, H-2))

out_image.save(output_file)
