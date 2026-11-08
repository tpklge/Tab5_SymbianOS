#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the bounded, stored-ZIP .t5pkg profile; no installation or device access."""
import argparse,hashlib,json,re,zipfile
from pathlib import Path

def build(source,output):
 source=Path(source).resolve();meta=json.loads((source/'package.json').read_text())
 for key in ('id','version','title','author','kind'):
  if not isinstance(meta.get(key),str) or any(c in meta[key] for c in '\n\r\t\0'):raise ValueError('invalid '+key)
 if not re.fullmatch('[a-z0-9_-]{1,32}',meta['id']) or not re.fullmatch(r'\d+(?:\.\d+)*',meta['version']):raise ValueError('invalid ID/version')
 if meta['kind'] not in ('app','theme'):raise ValueError('invalid kind')
 perms=meta.get('permissions',[])
 if len(set(perms))!=len(perms) or any(p not in ('documents','media','camera') for p in perms):raise ValueError('unsupported permissions')
 files={};aliases=set()
 for name in meta['files']:
  if not re.fullmatch('[A-Za-z0-9_.-]{1,100}',name) or name in ('.','..','manifest.txt') or name.lower() in aliases:raise ValueError('flat, unique filenames required')
  aliases.add(name.lower());p=source/name
  if p.is_symlink() or p.resolve().parent!=source:raise ValueError('links forbidden')
  data=p.read_bytes()
  if len(data)>1024*1024 or (name.endswith('.lua') and (len(data)>65536 or data[:1]==b'\x1b')):raise ValueError('asset/source limit')
  files[name]=data
 required='main.lua' if meta['kind']=='app' else 'theme.txt'
 if required not in files or ('theme.txt' in files and 'main.lua' in files) or len(files)>32:raise ValueError('invalid package entry')
 manifest=['format=1']+[f'{k}={meta[k]}' for k in ('id','version','title','author','kind')]+['api_min=4','api_max=4','orientation=landscape','memory=1048576','instructions=100000','permissions='+','.join(perms)]
 manifest += [f'file={name}|{hashlib.sha256(data).hexdigest()}' for name,data in sorted(files.items())]
 files['manifest.txt']=('\n'.join(manifest)+'\n').encode()
 if sum(map(len,files.values()))>4*1024*1024:raise ValueError('unpacked limit')
 output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(output,'w',zipfile.ZIP_STORED) as archive:
  for name,data in sorted(files.items()):
   info=zipfile.ZipInfo(name,(2026,1,1,0,0,0));info.compress_type=zipfile.ZIP_STORED;info.external_attr=0o100644<<16;archive.writestr(info,data)
 if output.stat().st_size>4*1024*1024:output.unlink();raise ValueError('archive limit')
 print(output)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();build(a.source,a.output)
