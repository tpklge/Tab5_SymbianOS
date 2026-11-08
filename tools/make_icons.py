#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Rasterize our original SVG artwork for the SD; never embed assets in firmware."""
from pathlib import Path
from io import BytesIO
import struct
import cairosvg
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
shapes={
 'notes': ('#f1c64e','#a97319','<path d="M25 19h49v59H25z" fill="#fff2b6"/><path d="M34 34h30M34 45h30M34 56h23" stroke="#a68035" stroke-width="3"/><path d="M22 22h54" stroke="#fff" stroke-width="4"/>'),
 'calculator': ('#9facb7','#445363','<rect x="24" y="15" width="49" height="66" rx="5" fill="#25323d"/><path d="M31 23h35v17H31z" fill="#c4d9b3"/><path d="M32 49h8m6 0h8m6 0h5M32 61h8m6 0h8m6 0h5M32 73h8m6 0h8m6 0h5" stroke="#e4e8ec" stroke-width="6"/>'),
 'contacts': ('#68a6dc','#275682','<rect x="20" y="17" width="55" height="64" rx="5" fill="#c8e9ff"/><path d="M20 17v64M75 30h7m-7 16h7m-7 16h7" stroke="#163c67" stroke-width="6"/><circle cx="48" cy="38" r="10" fill="#4b7d9e"/><path d="M31 68q1-22 17-22t17 22" fill="#4b7d9e"/>'),
 'agenda': ('#e76954','#8d2f2c','<rect x="19" y="21" width="59" height="58" rx="5" fill="#fff1e7"/><path d="M19 34h59" stroke="#ad3435" stroke-width="12"/><path d="M33 15v15m30-15v15" stroke="#d8dee3" stroke-width="6"/><text x="48" y="68" fill="#4d545e" font-size="31" text-anchor="middle" font-family="sans-serif">21</text>'),
 'clock': ('#aaaaca','#535775','<circle cx="48" cy="49" r="31" fill="#f0f2f3" stroke="#c5cbd4" stroke-width="4"/><path d="M48 27v23l17 10" fill="none" stroke="#344257" stroke-width="5" stroke-linecap="round"/><circle cx="48" cy="49" r="4" fill="#db6046"/>'),
 'converter': ('#8ebb7a','#3e693e','<path d="M22 34h45l-9-9m9 9-9 9M74 63H29l9-9m-9 9 9 9" fill="none" stroke="#f1f8df" stroke-width="8" stroke-linejoin="round"/>'),
 'files': ('#e7bd59','#947127','<path d="M17 30v47h63V32H48l-7-10H19z" fill="#ffe3a0" stroke="#ad863d" stroke-width="3"/><path d="M16 42h67l-7 37H22z" fill="#f4c660"/><path d="M23 47h51" stroke="#fff0bd" stroke-width="3"/>'),
 'backup': ('#70b4c6','#326276','<path d="M23 31h50v47H23z" fill="#d8e4e8"/><path d="M30 38h36v14H30z" fill="#386274"/><path d="M48 18v45m-13-13 13 13 13-13" fill="none" stroke="#fff" stroke-width="7"/>'),
 'profiles': ('#b88ccc','#613f7c','<path d="M25 28h46M25 49h46M25 70h46" stroke="#ece5f6" stroke-width="5"/><path d="M37 19v18m22 3v18M42 61v18" stroke="#fff" stroke-width="9"/>'),
 'menu': ('#77accc','#335770',''.join(f'<rect x="{x}" y="{y}" width="16" height="16" rx="3" fill="#e6f2ff"/>' for y in (21,43,65) for x in (21,43,65)))
}
shapes.update({
 'music':('#79b8d6','#31577e','<path d="M39 67V28l31-7v39" fill="none" stroke="#eff9ff" stroke-width="7"/><ellipse cx="29" cy="68" rx="13" ry="10" fill="#fff"/><ellipse cx="60" cy="61" rx="13" ry="10" fill="#fff"/>'),
 'recorder':('#df8179','#853b4a','<rect x="39" y="18" width="20" height="38" rx="10" fill="#fff1eb"/><path d="M30 43v5a19 19 0 0 0 38 0v-5M49 67v13m-14 0h28" fill="none" stroke="#fff1eb" stroke-width="5"/>'),
 'gallery':('#c0a8d2','#5d4b7f','<rect x="18" y="22" width="61" height="54" rx="4" fill="#eaf1fa"/><circle cx="63" cy="37" r="8" fill="#f2c659"/><path d="m21 69 20-24 15 18 9-10 11 16z" fill="#639575"/>'),
 'camera':('#a0adbb','#455368','<path d="M20 30h16l6-10h19l6 10h11v45H20z" fill="#e4eaf1"/><circle cx="49" cy="52" r="19" fill="#3b546e"/><circle cx="49" cy="52" r="12" fill="#6bb0ca"/><circle cx="44" cy="47" r="4" fill="#cceefa"/>'),
 'installer':('#d9af75','#806137','<path d="m19 39 29-17 29 17v33L48 87 19 72z" fill="#e6bd84" stroke="#694f33" stroke-width="2"/><path d="M48 83V53L22 39m26 14 26-14" fill="none" stroke="#9e7449" stroke-width="3"/><path d="M48 13v38m-11-10 11 11 11-11" fill="none" stroke="#fff8e6" stroke-width="7"/>')
})
shapes.update({
 'energy':('#8bbd6e','#3c6742','<rect x="22" y="30" width="51" height="38" rx="5" fill="none" stroke="#fff" stroke-width="6"/><path d="M75 41h6v16h-6M32 40h8v18h-8m14-18h8v18h-8m14-18h5v18h-5" fill="#fff"/>'),
 'network':('#74bacc','#32667a','<path d="M17 37q31-27 62 0M27 49q21-18 42 0M38 61q10-9 20 0" fill="none" stroke="#fff" stroke-width="7"/><circle cx="48" cy="75" r="6" fill="#fff"/>'),
 'feeds':('#e9b16d','#985933','<circle cx="25" cy="71" r="7" fill="#fff"/><path d="M23 47a26 26 0 0 1 26 26M23 24a49 49 0 0 1 49 49" fill="none" stroke="#fff" stroke-width="8"/>')
})
controls={
 'volume_down':'<path d="M18 38h14l20-17v54L32 58H18z" fill="#edf5ff"/><path d="M63 48h22" stroke="#edf5ff" stroke-width="6"/>',
 'volume_up':'<path d="M18 38h14l20-17v54L32 58H18z" fill="#edf5ff"/><path d="M63 48h22M74 37v22" stroke="#edf5ff" stroke-width="6"/>',
 'play':'<path d="m32 19 43 29-43 29z" fill="#edf5ff"/>',
 'pause':'<path d="M26 20h15v56H26zm29 0h15v56H55z" fill="#edf5ff"/>',
 'stop':'<rect x="24" y="24" width="48" height="48" rx="4" fill="#edf5ff"/>',
 'next':'<path d="m20 24 34 24-34 24zm38 0h10v48H58z" fill="#edf5ff"/>',
 'prev':'<path d="m76 24-34 24 34 24zM28 24h10v48H28z" fill="#edf5ff"/>',
 'record':'<circle cx="48" cy="48" r="28" fill="#f14f55"/>',
 'photo':'<circle cx="48" cy="48" r="33" fill="none" stroke="#fff" stroke-width="6"/><circle cx="48" cy="48" r="24" fill="#f4f6fa"/>',
 'video':'<path d="M16 28h43v40H16zm43 12 22-12v40L59 56z" fill="#edf5ff"/>',
 'save':'<path d="M22 16h45l10 10v54H22z" fill="none" stroke="#edf5ff" stroke-width="6"/><path d="M34 17v22h28V17M34 77V54h30v23" fill="none" stroke="#edf5ff" stroke-width="5"/>',
 'new':'<path d="M48 18v60M18 48h60" stroke="#edf5ff" stroke-width="8"/>',
 'list':'<path d="M31 25h47M31 48h47M31 71h47M17 25h5m-5 23h5m-5 23h5" stroke="#edf5ff" stroke-width="6"/>',
 'trash':'<path d="M27 29h44l-5 50H32zM20 21h58M37 13h23M40 38v29m15-29v29" fill="none" stroke="#edf5ff" stroke-width="5"/>',
 'edit':'<path d="m25 63 39-41 13 13-39 41-17 6z" fill="none" stroke="#edf5ff" stroke-width="6"/>',
 'back':'<path d="M73 48H22m24-24L22 48l24 24" fill="none" stroke="#edf5ff" stroke-width="7"/>',
}
for name,shape in controls.items():shapes['ui_'+name]=('#fff','#fff',shape)
out=bytearray(b'T5IC'+struct.pack('<HH',96,len(shapes)))
for name,(a,b,shape) in shapes.items():
 svg=f'''<svg xmlns="http://www.w3.org/2000/svg" width="96" height="96" viewBox="0 0 96 96"><defs><linearGradient id="g" x2="0" y2="1"><stop stop-color="{a}"/><stop offset="1" stop-color="{b}"/></linearGradient></defs><rect x="5" y="6" width="86" height="87" rx="19" fill="#111820" opacity=".5"/><rect x="5" y="3" width="86" height="86" rx="19" fill="url(#g)" stroke="#e7f1ff" stroke-opacity=".35"/>{shape}<path d="M20 6h56q12 0 12 15v12Q48 16 8 37V21Q8 6 20 6" fill="#fff" opacity=".13"/></svg>'''
 if name.startswith('ui_'):svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="96" height="96" viewBox="0 0 96 96">{shape}</svg>'
 (ROOT/'artwork'/f'{name}.svg').write_text(svg)
 im=Image.open(BytesIO(cairosvg.svg2png(bytestring=svg.encode()))).convert('RGBA')
 rgb=bytearray();alpha=bytearray()
 for r,g,b,a in im.get_flattened_data():
  rgb+=struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3));alpha.append(a)
 out+=name.encode().ljust(16,b'\0')+rgb+alpha
(ROOT/'sdcard/SymbianOS/system/releases/0.7.1/icons.bin').write_bytes(out)
print(f'{len(shapes)} original icons, {len(out)} SD bytes')
