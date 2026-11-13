#!/usr/bin/env python3
"""Prepare reviewed local release artifacts. Never connects to a device."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SD = ROOT / 'sdcard' / 'SymbianOS'
DIST = ROOT / 'sist' / '1.0.0'
VERSION = '1.0.0'
RUNTIME_VERSION = '1.0.0'

def validate_template(sd):
    # Runtime data can occur anywhere outside the versioned system resources.
    # Never depend on a growing blacklist of applications with private files.
    for path in sd.rglob('*'):
        if path.is_symlink():
            raise ValueError(f'Link forbidden in distribution: {path}')
        if not path.is_file() or path.name == '.DS_Store' or path.name.startswith('._'):
            continue
        rel = path.relative_to(sd)
        if rel == Path('boot/manifest.txt') or rel.parts[:2] == ('system', 'releases'):
            continue
        if path.name == '.gitkeep' and path.stat().st_size == 0:
            continue
        raise ValueError(f'Private or unexpected file in template SD: {rel}')

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--launcher-only', action='store_true', help='Generate only the application BIN and SymbianOS folder for Launcher WebGUI')
    args = parser.parse_args()
    args.launcher_only = True  # Runtime updates belong exclusively to Launcher.
    validate_template(SD)
    subprocess.run([sys.executable, str(ROOT/'tools/package_sd.py'), '--verify'], check=True)
    desc=json.loads((ROOT/'build/project_description.json').read_text())
    if desc['project_version']!=RUNTIME_VERSION:
        raise SystemExit('Build version mismatch; run ./tools/idf.sh build first.')
    config=json.loads((ROOT/'build/config/sdkconfig.json').read_text())
    if config.get('ESP_HOSTED_SDIO_OPTIMIZATION_RX_STREAMING_MODE') is not True:
        raise SystemExit('Refusing packet-RX image: factory C6 requires streaming RX.')
    if config.get('BT_NIMBLE_NVS_PERSIST') or config.get('NIMBLE_NVS_PERSIST'):
        raise SystemExit('Refusing BLE credential persistence outside the SD.')
    if config.get('WIFI_RMT_NVS_ENABLED') or config.get('ESP_WIFI_NVS_ENABLED'):
        raise SystemExit('Refusing Wi-Fi credential persistence outside the SD.')
    app=ROOT/'build/tab5symbian_runtime.bin'
    if not app.is_file() or app.read_bytes()[:1]!=b'\xe9':
        raise SystemExit('Missing ESP application image.')
    for folder in ('main','components/platform','components/html_engine','components/javascript'):
        for p in (ROOT/folder).rglob('*'):
            if p.suffix in ('.c','.cpp','.hpp','.h') and p.stat().st_mtime>app.stat().st_mtime:
                raise SystemExit(f'Source newer than binary: {p}; rebuild first.')
    for relative in ('CMakeLists.txt','main/CMakeLists.txt','tools/patch_httpd.py','tools/patch_http_client.py','sdkconfig.defaults'):
        source=ROOT/relative
        if source.stat().st_mtime>app.stat().st_mtime:
            raise SystemExit(f'Build integration newer than binary: {source}; rebuild first.')
    if config.get('COMPILER_CXX_EXCEPTIONS_EMG_POOL_SIZE',0)<4096:
        raise SystemExit('Browser error handling requires the C++ emergency exception pool.')
    if config.get('FREERTOS_TASK_CREATE_ALLOW_EXT_MEM') is not True:
        raise SystemExit('File-transfer task requires external stack support.')
    if app.stat().st_size>4096*1024:
        raise SystemExit('Application exceeds project slot; inspect the size before distributing.')
    DIST.mkdir(parents=True,exist_ok=True)
    staged=DIST/'launcher'
    if staged.exists():shutil.rmtree(staged)
    (staged/'SymbianOS/boot').mkdir(parents=True)
    shutil.copy2(SD/'boot/manifest.txt',staged/'SymbianOS/boot/manifest.txt')
    release=SD/'system/releases'/VERSION
    shutil.copytree(release,staged/'SymbianOS/system/releases'/VERSION, ignore=shutil.ignore_patterns('.DS_Store','._*'))
    shutil.copy2(app,staged/'SymbianOS-runtime.bin')
    # Only create missing directories. Never ship user documents/preferences.
    for prefix in ('data','config','home','backups','cache','logs','tmp','update','meta','apps','themes','fonts'):
        for d in [SD/prefix]+list((SD/prefix).rglob('*')):
            if d.is_dir():(staged/'SymbianOS'/d.relative_to(SD)).mkdir(parents=True,exist_ok=True)
    if args.launcher_only:
        # No partition/bootloader images, SDKs or retention cleanup in this mode.
        (staged/'SHA256SUMS').write_text(digest(staged/'SymbianOS-runtime.bin')+'  SymbianOS-runtime.bin\n')
        subprocess.run([sys.executable,str(ROOT/'tools/package_sd.py'),'--root',str(staged/'SymbianOS'),'--verify'],check=True)
        subprocess.run([sys.executable,str(ROOT/'tools/prune_releases.py')],check=True)
        print(f'Prepared Launcher WebGUI: {staged}/SymbianOS-runtime.bin and {staged}/SymbianOS/')
        return

if __name__=='__main__':main()
