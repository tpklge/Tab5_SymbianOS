#!/usr/bin/env python3
"""Inspect an offline SymbianOS folder and select a release. Never accesses serial/flash."""
import argparse
import hashlib
import json
from pathlib import Path
import os
import re
from package_sd import files, LIMIT

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True, help='Path to the SymbianOS folder on an offline SD/copy')
    parser.add_argument('--release', help='Release folder to activate; omit to list')
    parser.add_argument('--api', type=int, choices=range(1, 15), help='Explicit API for legacy releases without metadata')
    args = parser.parse_args()
    root = args.root.resolve()
    files(root)  # Confinement, aliases and decimal 4 GB quota.
    releases = root / 'system/releases'
    if not releases.is_dir():
        raise SystemExit('Missing system/releases directory')
    if not args.release:
        for item in sorted(releases.iterdir()):
            if item.is_dir() and (item / 'release.json').is_file():
                print(item.name)
        return
    if not re.fullmatch(r'[a-z0-9][a-z0-9.-]*', args.release):
        raise SystemExit('Invalid release name')
    release = releases / args.release
    catalog = json.loads((release / 'release.json').read_text())
    api = catalog.get('api', args.api)
    if api not in range(1, 15):
        raise SystemExit('Legacy release: specify --api matching its runtime documentation')
    if args.api and args.api != api:
        raise SystemExit('API conflicts with release metadata')
    entries = ['format=1', f'api_min={api}', f'api_max={api}', 'schema=1', f'release={args.release}']
    for prefix, field in [('app', 'apps'), ('grant', 'grants'), ('cap', 'capabilities')]:
        entries += [f'{prefix}.{key}={value}' for key, value in catalog[field].items()]
    names = {p.name for p in release.iterdir() if p.is_file() and p.name != '.gitkeep'}
    if not (set(catalog['apps'].values()) | {'theme.ttf', 'keyboard.map', 'onscreen.keys', 'chrome.lua'}).issubset(names):
        raise SystemExit('Missing essential release files')
    for name in sorted(names):
        entries.append(f'file={name}|{hashlib.sha256((release / name).read_bytes()).hexdigest()}')
    payload = ('\n'.join(entries) + '\n').encode()
    manifest = root / 'boot/manifest.txt'
    old = manifest.read_bytes() if manifest.is_file() else b''
    if sum(p.stat().st_size for p in files(root)) + len(old) + len(payload) > LIMIT:
        raise SystemExit('Insufficient quota for recoverable manifest replacement')
    for name, data in [('recovery-backup.txt', old), ('recovery-candidate.txt', payload)]:
        with (manifest.parent / name).open('wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
    os.replace(manifest.parent / 'recovery-candidate.txt', manifest)
    for name in ('pending.txt', 'attempt.txt'):
        (manifest.parent / name).unlink(missing_ok=True)
    print(f'Selected {args.release} / API {api}; previous manifest saved as boot/recovery-backup.txt. Runtime must support this API.')

if __name__ == '__main__':
    main()
