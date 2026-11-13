#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the unsigned, uncompressed Tab5 SIS v9 application profile."""
import argparse, binascii, hashlib, json, re, struct
from pathlib import Path

def u32(n): return struct.pack('<I', n)
def field(kind, data): return u32(kind)+u32(len(data))+data+b'\0'*(-len(data)%4)
def array(kind, values): return field(2,u32(kind)+b''.join(v[4:] for v in values))
def text(value):
 if any(ord(c)<32 or ord(c)>0xffff or 0xd800<=ord(c)<=0xdfff for c in value): raise ValueError('UCS-2 text required')
 return field(1,value.encode('utf-16le'))
def compressed(data): return field(3,struct.pack('<IQ',0,len(data))+data)
def checksum(data): return binascii.crc_hqx(data[1::2],0)<<16 | binascii.crc_hqx(data[::2],0)
def build(source, output):
 source=Path(source).resolve(); meta=json.loads((source/'sis.json').read_text())
 if not {'id','uid','version','title','author','license','files'}<=set(meta) or set(meta)-{'id','uid','version','title','author','license','files','dependencies','permissions','schema','migration'}: raise ValueError('sis.json keys do not match profile')
 for key in ('id','version','title','author','license'):
  if not isinstance(meta[key],str) or not meta[key] or any(c in meta[key] for c in '\r\n\t\0'):raise ValueError('invalid '+key)
 if not re.fullmatch('[a-z0-9_-]{1,32}',meta['id']) or meta['id'] in ('shell', 'services', 'notes', 'calculator', 'contacts', 'agenda', 'clock', 'converter', 'files', 'backup', 'profiles', 'music', 'recorder', 'gallery', 'camera', 'installer', 'energy', 'network', 'feeds', 'email', 'browser', 'memory', 'bench', 'mesh_ble', 'mesh_rak', 'settings', 'lock', 'updates'):raise ValueError('invalid id')
 if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)',meta['version']):raise ValueError('three-part version required')
 version=tuple(map(int,meta['version'].split('.')))
 if max(version)>2147483647:raise ValueError('version overflow')
 uid=meta['uid'];uid=int(uid,0) if isinstance(uid,str) else uid
 if type(uid) is not int or not 0xe0000000<=uid<=0xefffffff:raise ValueError('local test UID E0000000..EFFFFFFF required')
 if len(meta['title'].encode())>80 or len(meta['author'].encode())>80 or meta['license']!='MIT':raise ValueError('profile requires bounded metadata and MIT license')
 profile=2 if set(meta)&{'dependencies','permissions','schema','migration'} else 1
 permissions=meta.get('permissions',[])
 if not isinstance(permissions,list) or len(set(permissions))!=len(permissions) or any(v not in ('documents','media','camera') for v in permissions):raise ValueError('unsupported permissions')
 schema=meta.get('schema',1)
 if type(schema) is not int or not 1<=schema<=9999:raise ValueError('schema must be 1..9999')
 def version_tuple(value):
  if not isinstance(value,str) or not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)',value):raise ValueError('dependency version')
  parts=tuple(map(int,value.split('.')))
  if max(parts)>2147483647:raise ValueError('dependency version overflow')
  return parts
 dependencies=[];seen=set()
 if not isinstance(meta.get('dependencies',[]),list) or len(meta.get('dependencies',[]))>16:raise ValueError('dependency limit')
 for dep in meta.get('dependencies',[]):
  if not isinstance(dep,dict) or set(dep)!={'uid','min','max'}:raise ValueError('dependency keys')
  duid=int(dep['uid'],0) if isinstance(dep['uid'],str) else dep['uid']
  if type(duid) is not int or not 0xe0000000<=duid<=0xefffffff or duid==uid or duid in seen:raise ValueError('dependency UID collision/cycle')
  lo,hi=version_tuple(dep['min']),version_tuple(dep['max'])
  if lo>hi:raise ValueError('dependency range')
  seen.add(duid);dependencies.append((duid,dep['min'],dep['max']))
 dependencies.sort()
 files={};aliases=set()
 if not isinstance(meta['files'],list) or not 2<=len(meta['files'])<=32:raise ValueError('2..32 files required')
 for name in meta['files']:
  if not isinstance(name,str) or not re.fullmatch('[A-Za-z0-9_.-]{1,100}',name) or name.endswith('.') or name in ('.','..') or '.t5-' in name or name.lower() in aliases or name.lower()=='manifest.txt':raise ValueError('flat, unique filenames required')
  aliases.add(name.lower());path=source/name
  if path.is_symlink() or path.resolve().parent!=source:raise ValueError('links forbidden')
  data=path.read_bytes()
  if len(data)>1024*1024 or (name.endswith('.lua') and (len(data)>65536 or data[:1]==b'\x1b')):raise ValueError('file limit/source-only Lua')
  if name.endswith('.lua'):data.decode('utf-8')
  if name.lower().endswith(('.exe','.dll','.bin')):raise ValueError('native binaries not supported')
  files[name]=data
 if not {'main.lua','LICENSE.txt'}<=files.keys() or 'theme.txt' in files:raise ValueError('main.lua and LICENSE.txt required')
 manifest=['format=1',f'sis_profile={profile}',f'uid={uid}','license=MIT']+[f'{k}={meta[k]}' for k in ('id','version','title','author')]+['kind=app','api_min=4','api_max=4','runtime_api=14','orientation=landscape','memory=1048576','instructions=100000','permissions='+','.join(permissions)]
 if profile==2:
  manifest += [f'schema={schema}']
  migration=meta.get('migration')
  if migration is not None:
   if not isinstance(migration,dict) or set(migration)!={'from','rename','defaults'} or type(migration['from']) is not int or not 1<=migration['from']<=9999 or migration['from']==schema:raise ValueError('migration contract')
   if 'migration.tsv' in aliases:raise ValueError('migration.tsv is generated')
   def filename(name):
    if not isinstance(name,str) or not re.fullmatch('[A-Za-z0-9_.-]{1,100}',name) or name in ('.','..') or name.endswith('.') or '.t5-' in name:raise ValueError('migration filename')
    return name
   commands=['T5MIGRATE1']
   if not isinstance(migration['rename'],list) or not isinstance(migration['defaults'],dict) or len(migration['rename'])+len(migration['defaults'])>32:raise ValueError('migration limit')
   for pair in migration['rename']:
    if not isinstance(pair,list) or len(pair)!=2 or pair[0].lower()==pair[1].lower():raise ValueError('rename pair')
    commands.append('rename\t'+filename(pair[0])+'\t'+filename(pair[1]))
   for name,value in sorted(migration['defaults'].items()):
    if not isinstance(value,str) or len(value.encode())>4096:raise ValueError('default value')
    commands.append('default\t'+filename(name)+'\t'+value.encode().hex())
   files['migration.tsv']=('\n'.join(commands)+'\n').encode()
   if len(files)>32 or len(files['migration.tsv'])>16384:raise ValueError('migration size')
   manifest += [f"migrate_from={migration['from']}",'migration=migration.tsv']
  manifest += [f'depends={duid}|{lo}|{hi}' for duid,lo,hi in dependencies]
 manifest += [f'file={name}|{hashlib.sha256(data).hexdigest()}' for name,data in sorted(files.items())]
 files['manifest.txt']=('\n'.join(manifest)+'\n').encode()
 if sum(map(len,files.values()))>4*1024*1024:raise ValueError('payload exceeds 4 MiB')
 descriptions=[];data=[]
 for index,(name,content) in enumerate(sorted(files.items())):
  target='!:\\SymbianOS\\apps\\'+meta['id']+'\\'+meta['version']+'\\'+name
  descriptions.append(field(24,text(target)+text('')+field(25,u32(1)+field(37,hashlib.sha1(content).digest()))+struct.pack('<IIQQI',1,0,len(content),len(content),index)))
  data.append(field(32,compressed(content)))
 info=field(14,field(9,u32(uid))+text(meta['author'])+array(1,[text(meta['title'])])+array(1,[text(meta['author'])])+field(4,struct.pack('<iii',*version))+field(8,field(6,struct.pack('<HBB',2026,0,1))+field(7,b'\0\0\0'))+b'\0\0')
 controller=field(13,info+field(16,array(33,[]))+field(15,array(11,[field(11,u32(1))]))+field(17,array(18,[])+array(18,[field(18,field(9,u32(duid))+field(5,field(4,struct.pack('<iii',*version_tuple(lo)))+field(4,struct.pack('<iii',*version_tuple(hi))))+array(1,[text(f'{duid:08x}')])) for duid,lo,hi in dependencies]))+field(19,array(20,[]))+field(28,array(24,descriptions)+array(13,[])+array(26,[]))+field(40,u32(0)))
 if len(controller)>65536:raise ValueError('controller exceeds 64 KiB')
 header=struct.pack('<III',0x10201a7a,0,uid)
 package=header+u32(checksum(header))+field(12,compressed(controller)+field(30,array(31,[field(31,array(32,data))])))
 if len(package)>4*1024*1024:raise ValueError('package exceeds 4 MiB')
 output=Path(output)
 if output.suffix.lower()!='.sis':raise ValueError('unsigned output must end in .sis')
 output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(package)
 report={'profile':f'tab5-sis-{profile}','signed':False,'uid':f'0x{uid:08x}','id':meta['id'],'version':meta['version'],'bytes':len(package),'sha256':hashlib.sha256(package).hexdigest(),'files':sorted(files),'permissions':permissions,'schema':schema,'dependencies':[{'uid':duid,'min':lo,'max':hi} for duid,lo,hi in dependencies]}
 output.with_suffix('.sis.json').write_text(json.dumps(report,indent=2)+'\n')
 return report
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('source');parser.add_argument('output');args=parser.parse_args()
 try: print(json.dumps(build(args.source,args.output),indent=2))
 except (ValueError,OSError,KeyError,TypeError) as error:parser.exit(1,str(error)+'\n')
