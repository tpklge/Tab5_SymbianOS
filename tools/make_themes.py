#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original geometric wallpapers; raw RGB565 lives only on SD."""
from pathlib import Path
import math,struct
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'sdcard/SymbianOS/system/releases/1.0.0'
for name in ('aurora','solar','prisma'):
 im=Image.new('RGB',(320,180));d=ImageDraw.Draw(im)
 colors={'aurora':('#132238','#327b8c','#5361aa','#82cebd'), 'solar':('#6b3540','#d36b45','#edb95f','#ba4b57'), 'prisma':('#282846','#6159a6','#c45d94','#5bafba')}[name]
 for y in range(180):
  base=tuple(int(colors[0][i:i+2],16) for i in (1,3,5));t=y/180
  d.line((0,y,320,y),fill=tuple(min(255,int(c*(0.65+0.5*t))) for c in base))
 if name=='aurora':
  for i in range(6):
   x=i*70-90;d.polygon([(x,-20),(x+100,-20),(x+260,180),(x+175,180)],fill=colors[1+i%3])
  d.polygon([(0,105),(190,180),(0,180)],fill='#152c40')
 elif name=='solar':
  for r in (105,80,57,35):
   k=(105-r)//20;d.ellipse((215-r,50-r,215+r,50+r),fill=colors[1+k%3])
  d.polygon([(0,130),(100,62),(220,180),(0,180)],fill='#a24d4c')
  d.polygon([(75,180),(250,90),(320,140),(320,180)],fill='#713b53')
 else:
  for y in range(-1,4):
   for x in range(-1,6):
    cx=x*68+(y%2)*34;cy=y*63
    d.polygon([(cx,cy),(cx+64,cy),(cx+32,cy+60)],fill=colors[(x+y)%4])
 raw=bytearray(b'T5BG'+struct.pack('<HH',320,180))
 for r,g,b in im.get_flattened_data():raw+=struct.pack('<H',(r>>3)<<11|(g>>2)<<5|(b>>3))
 (OUT/f'wallpaper-{name}.rgb').write_bytes(raw)
 im.resize((1280,720)).save(ROOT/'artwork'/f'wallpaper-{name}.png')
 print(name,len(raw),'SD bytes')
