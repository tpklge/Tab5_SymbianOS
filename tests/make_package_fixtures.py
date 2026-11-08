# SPDX-License-Identifier: MIT
import sys,zipfile,json,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from make_package import build
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
build(root/'sdk/hello',out/'hello.t5pkg');build(root/'sdk/theme-graphite',out/'theme.t5pkg')
with zipfile.ZipFile(out/'hello.t5pkg') as z:base={n:z.read(n) for n in z.namelist()}
def save(name,files,compression=zipfile.ZIP_STORED):
 with zipfile.ZipFile(out/name,'w',compression=compression) as z:
  for n,b in files.items():z.writestr(n,b)
save('compressed.t5pkg',base,zipfile.ZIP_DEFLATED)
save('traversal.t5pkg',{**base,'../escape':b'bad'})
save('alias.t5pkg',{**base,'MAIN.LUA':b'bad'})
save('bad-hash.t5pkg',{**base,'main.lua':base['main.lua']+b'bad'})
save('incompatible.t5pkg',{**base,'manifest.txt':base['manifest.txt'].replace(b'api_min=4',b'api_min=99')})
save('permission.t5pkg',{**base,'manifest.txt':base['manifest.txt'].replace(b'permissions=',b'permissions=install')})
updated={**base,'main.lua':base['main.lua'].replace(b'Ol',b'Ol')+b'\n-- version two\n'}
updated['manifest.txt']=base['manifest.txt'].replace(b'version=1.0.0',b'version=2.0.0').replace(hashlib.sha256(base['main.lua']).hexdigest().encode(),hashlib.sha256(updated['main.lua']).hexdigest().encode())
save('hello-v2.t5pkg',updated)
corrupt={**base,'main.lua':b'\x1bLua'+b'x'*20};corrupt['manifest.txt']=base['manifest.txt'].replace(hashlib.sha256(base['main.lua']).hexdigest().encode(),hashlib.sha256(corrupt['main.lua']).hexdigest().encode());save('bytecode.t5pkg',corrupt)

import struct
raw=bytearray((out/'hello.t5pkg').read_bytes());at=raw.index(b'PK\x01\x02');struct.pack_into('<I',raw,at+42,0xfffffff0);(out/'overflow.t5pkg').write_bytes(raw)
def custom(name,identifier,files):
 manifest=base['manifest.txt'].decode().split('file=')[0].replace('id=hello','id='+identifier)
 manifest+=''.join('file='+n+'|'+hashlib.sha256(b).hexdigest()+'\n' for n,b in sorted(files.items()))
 save(name,{**files,'manifest.txt':manifest.encode()})
custom('guard.t5pkg','guard',{'main.lua':b'function event() sys.write("/data/notes/protected.txt","denied") return {title="guard"} end'})
custom('picture-app.t5pkg','picture_app',{'main.lua':b'local chrome=sys.module("chrome"); local ok,why=sys.media("image","asset:picture.png"); assert(ok,why); function event() return chrome.view{title="Own asset",layout="image",buttons={}} end','picture.png':(root/'tests/fixtures/picture.png').read_bytes()})
