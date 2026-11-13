#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Package unsigned SIS SDK and physical-test fixtures. No device operations."""
import argparse, hashlib, importlib.util, pathlib, zipfile, json, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('make_sis_testkit',ROOT/'tools/make_sis_testkit.py');kit=importlib.util.module_from_spec(spec);spec.loader.exec_module(kit)
def archive(output,items):
 with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
  for name,path in sorted(items):
   info=zipfile.ZipInfo(name,(2026,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16;z.writestr(info,path.read_bytes())
def release(destination):
 destination=pathlib.Path(destination);destination.mkdir(parents=True,exist_ok=True);generated=kit.build(destination/'sis-test-kit').resolve()
 entries=json.loads((generated/'KIT-MANIFEST.json').read_text())
 for name in entries:
  p=generated/name
  if p.is_symlink() or p.resolve()!=p.absolute() or name.startswith('/') or '..' in pathlib.PurePosixPath(name).parts:raise ValueError('unsafe generated artifact')
 testzip=destination/'Tab5-SIS-test-kit.zip';archive(testzip,[(name,generated/name) for name in entries])
 files=[('LICENSE',ROOT/'LICENSE')]
 for name in ['tools/make_sis.py','tools/make_sis_testkit.py','tools/inspect_sis.cpp','docs/SIS_SDK.md','docs/ROTEIRO_FISICO_SIS.md','docs/PERFIL_SIS_1.md','docs/SIS_CICLO_DE_VIDA.md']:
  files.append((name,ROOT/name))
 files += [('examples/'+name[len('sources/'):],generated/name) for name in entries if name.startswith('sources/')]
 sdk=destination/'Tab5-SIS-SDK.zip';archive(sdk,files)
 (destination/'SIS-SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in [sdk,testzip]))
 return sdk,testzip
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',default=str(ROOT/'sist'/'1.0.0'));args=parser.parse_args()
 for path in release(args.output):print(path)
 if pathlib.Path(args.output).resolve()==(ROOT/"sist/1.0.0").resolve():subprocess.run([sys.executable,str(ROOT/"tools/prune_releases.py")],check=True)
