#!/usr/bin/env python3
"""Build/verify the external installation; never accesses a device."""
import argparse
import hashlib
import pathlib
import zipfile
import json

LIMIT = 4_000_000_000
RELEASE = '0.8.0'
def files(root):
    result=[]
    aliases=set()
    for p in root.rglob('*'):
        if p.is_symlink(): raise ValueError(f'link forbidden: {p}')
        if p.is_file():
            rel=p.relative_to(root)
            alias=str(rel).casefold()
            if alias in aliases:raise ValueError(f'case alias: {rel}')
            aliases.add(alias)
            for part in rel.parts:
                if '.t5-' in part or part in ('.','..') or part.endswith((' ','.')) or any(c in part for c in '\\:*?"<>|~'):
                    raise ValueError(f'invalid FAT path: {rel}')
            result.append(p)
    if sum(p.stat().st_size for p in result)>LIMIT: raise ValueError('quota exceeded')
    return sorted(result)
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[1]/'sdcard/SymbianOS')
    parser.add_argument('--verify',action='store_true')
    parser.add_argument('--zip',type=pathlib.Path)
    args=parser.parse_args();root=args.root.resolve();items=files(root)
    manifest=root/'boot/manifest.txt';release=root/'system/releases'/RELEASE
    catalog=json.loads((release/'release.json').read_text())
    if not args.verify:
        entries=['format=1','api_min=8','api_max=8','schema=1',f'release={RELEASE}']
        entries += [f'app.{key}={value}' for key,value in catalog['apps'].items()]
        entries += [f'grant.{key}={value}' for key,value in catalog['grants'].items()]
        entries += [f'cap.{key}={value}' for key,value in catalog['capabilities'].items()]
        for p in sorted(release.iterdir()):
            if p.is_file() and p.name!='.gitkeep': entries.append(f'file={p.name}|{hashlib.sha256(p.read_bytes()).hexdigest()}')
        manifest.write_text('\n'.join(entries)+'\n',encoding='utf-8')
    lines=manifest.read_text(encoding='utf-8').splitlines();verified=set()
    values={}
    for line in lines:
        if not line or line.startswith('#'):continue
        key,value=line.split('=',1)
        if key!='file':
            if key in values:raise ValueError(f'duplicate manifest key: {key}')
            values[key]=value
    for key,value in {'format':'1','api_min':'8','api_max':'8','schema':'1','release':RELEASE,'app.shell':'shell.lua','app.notes':'notes.lua','app.calculator':'calculator.lua'}.items():
        if values.get(key)!=value:raise ValueError(f'invalid manifest field: {key}')
    for line in lines:
        if line.startswith('file='):
            name,digest=line[5:].split('|');p=release/name
            if name in verified:raise ValueError(f'duplicate hash: {name}')
            if p.resolve().parent!=release.resolve():raise ValueError('invalid release path')
            if hashlib.sha256(p.read_bytes()).hexdigest()!=digest: raise ValueError(f'hash mismatch: {name}')
            verified.add(name)
    if not (set(catalog['apps'].values()) | {'theme.ttf','chrome.lua','keyboard.map','onscreen.keys','store.lua'}).issubset(verified): raise ValueError('missing essential files')
    for key,value in catalog['apps'].items():
        if values.get('app.'+key)!=value:raise ValueError(f'missing app {key}')
    items=files(root)
    if args.zip:
        if root in args.zip.resolve().parents:raise ValueError('ZIP must be outside source tree')
        args.zip.parent.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(args.zip,'w',zipfile.ZIP_DEFLATED) as archive:
            for d in sorted(root.rglob('*')):
                if d.is_dir():archive.writestr('SymbianOS/'+d.relative_to(root).as_posix()+'/',b'')
            for p in items: archive.write(p,'SymbianOS/'+p.relative_to(root).as_posix())
    print(f'Installation verified: {len(items)} files, {sum(p.stat().st_size for p in items)} / {LIMIT} bytes')
if __name__=='__main__':main()
