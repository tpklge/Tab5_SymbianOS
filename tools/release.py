#!/usr/bin/env python3
"""Prepare reviewed local release artifacts. Never connects to a device."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SD = ROOT / 'sdcard' / 'SymbianOS'
DIST = ROOT / 'dist'
VERSION = '0.7.1'
RUNTIME_VERSION = '0.7.2'

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    subprocess.run([sys.executable, str(ROOT/'tools/package_sd.py'), '--verify'], check=True)
    desc=json.loads((ROOT/'build/project_description.json').read_text())
    if desc['project_version']!=RUNTIME_VERSION:
        raise SystemExit('Build version mismatch; run ./tools/idf.sh build first.')
    app=ROOT/'build/tab5symbian_runtime.bin'
    if not app.is_file() or app.read_bytes()[:1]!=b'\xe9':
        raise SystemExit('Missing ESP application image.')
    for folder in ('main','components/platform'):
        for p in (ROOT/folder).rglob('*'):
            if p.suffix in ('.cpp','.hpp','.h') and p.stat().st_mtime>app.stat().st_mtime:
                raise SystemExit(f'Source newer than binary: {p}; rebuild first.')
    if app.stat().st_size>2*1024*1024:
        raise SystemExit('Application exceeds project slot; inspect the size before distributing.')
    DIST.mkdir(exist_ok=True)
    shutil.copy2(app,DIST/'SymbianOS-runtime.bin')
    subprocess.run([sys.executable,str(ROOT/'tools/package_sd.py'),'--zip',str(DIST/'SymbianOS-sdcard.zip')],check=True)
    staged=DIST/'launcher'
    if staged.exists():shutil.rmtree(staged)
    (staged/'SymbianOS/boot').mkdir(parents=True)
    shutil.copy2(SD/'boot/manifest.txt',staged/'SymbianOS/boot/manifest.txt')
    release=SD/'system/releases'/VERSION
    shutil.copytree(release,staged/'SymbianOS/system/releases'/VERSION)
    shutil.copy2(app,staged/'SymbianOS-runtime.bin')
    # Only create missing directories. Never ship user documents/preferences.
    for prefix in ('data','config','home','backups','cache','logs','tmp','update','meta','apps','themes','fonts'):
        for d in [SD/prefix]+list((SD/prefix).rglob('*')):
            if d.is_dir():(staged/'SymbianOS'/d.relative_to(SD)).mkdir(parents=True,exist_ok=True)
    update=DIST/f'SymbianOS-update-{VERSION}.zip'
    with zipfile.ZipFile(update,'w',zipfile.ZIP_DEFLATED)as z:
        for p in sorted((staged/'SymbianOS').rglob('*')):
            rel=p.relative_to(staged).as_posix()
            if p.is_dir():z.writestr(rel+'/',b'')
            else:z.write(p,rel)
    install=DIST/'runtime-installation';install.mkdir(exist_ok=True)
    args=json.loads((ROOT/'build/flasher_args.json').read_text());layout={}
    for offset,name in args['flash_files'].items():
        source=ROOT/'build'/name;shutil.copy2(source,install/source.name);layout[offset]=source.name
    (install/'flash-layout.json').write_text(json.dumps({'chip':'esp32p4','flash_settings':args['flash_settings'],'files':layout,'note':'Standalone layout only; not for upload through Launcher OTA.'},indent=2)+'\n')
    packages=DIST/'packages';packages.mkdir(exist_ok=True)
    for source,name in [('hello','hello-1.0.0'),('theme-graphite','graphite-1.0.0')]:
        subprocess.run([sys.executable,str(ROOT/'tools/make_package.py'),str(ROOT/'sdk'/source),str(packages/(name+'.t5pkg'))],check=True)
    sdk_zip=DIST/f'SymbianOS-SDK-{VERSION}.zip'
    with zipfile.ZipFile(sdk_zip,'w',zipfile.ZIP_DEFLATED)as z:
        for p in sorted((ROOT/'sdk').rglob('*')):
            if p.is_file():z.write(p,p.relative_to(ROOT).as_posix())
        for name in ['tools/make_package.py','docs/SDK.md','docs/API.md','docs/FASES_4_5.md','docs/FASE_6_0_7.md','docs/CORRECOES_0_7_1.md','docs/HOTFIX_0_7_2.md','LICENSE']:
            z.write(ROOT/name,name)
    notices=DIST/'licenses';notices.mkdir(exist_ok=True)
    for name in ['minimp3-LICENSE','stb-LICENSE','sources.json']:
        shutil.copy2(ROOT/'components/platform/vendor'/name,notices/name)
    for component in ('esp_hosted','esp_wifi_remote','eppp_link','esp_serial_slave_link'):
        source=ROOT/'managed_components'/('espressif__'+component)
        for notice in source.iterdir():
            if notice.is_file() and notice.name.lower().startswith(('license','copying','notice')):
                shutil.copy2(notice,notices/(component+'-'+notice.name))
    shutil.copy2(ROOT/'managed_components/espressif__esp_hosted/common/protobuf-c/LICENSE',notices/'protobuf-c-LICENSE')
    artifacts=[sdk_zip]+sorted(packages.glob('*.t5pkg'))+[DIST/'SymbianOS-runtime.bin',DIST/'SymbianOS-sdcard.zip',update]+sorted(install.glob('*.bin'))
    (DIST/'SHA256SUMS').write_text(''.join(digest(p)+'  '+str(p.relative_to(DIST))+'\n'for p in artifacts))
    (DIST/'release.json').write_text(json.dumps({'version':VERSION,'runtime_version':RUNTIME_VERSION,'api':7,'application_bytes':app.stat().st_size,'application_sha256':digest(app),'sd_logical_bytes':sum(p.stat().st_size for p in SD.rglob('*')if p.is_file()),'update_zip':update.name,'required_update':['application bin','boot/manifest.txt','system/releases/0.7.1','missing data/config directories'],'preserved':['existing user documents','existing configuration','previous SD releases']},indent=2)+'\n')
    print(f'Prepared application ({app.stat().st_size} bytes), full SD and data-preserving update package.')

if __name__=='__main__':main()
