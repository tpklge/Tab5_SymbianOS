#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util,pathlib,tempfile,zipfile,hashlib,subprocess,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('release_sis',ROOT/'tools/release_sis.py');release=importlib.util.module_from_spec(spec);spec.loader.exec_module(release)
class Distribution(unittest.TestCase):
 def test_sdk_and_kit(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=pathlib.Path(tmp);sdk,kit=release.release(root/'dist');first=[hashlib.sha256(p.read_bytes()).hexdigest() for p in [sdk,kit]]
   with zipfile.ZipFile(kit) as z:
    names=z.namelist();self.assertEqual(len([n for n in names if n.startswith('packages/') and n.endswith('.sis')]),20);self.assertFalse(any(n.endswith(('.sisx','.bin')) for n in names));self.assertIn('SHA256SUMS',names)
   with zipfile.ZipFile(sdk) as z:
    self.assertIn('docs/ROTEIRO_FISICO_SIS.md',z.namelist());self.assertIn('tools/make_sis.py',z.namelist());self.assertTrue(all(not n.startswith('/') and '..' not in pathlib.PurePosixPath(n).parts for n in z.namelist()));z.extractall(root/'sdk')
   subprocess.run([sys.executable,'-B',str(root/'sdk/tools/make_sis.py'),str(root/'sdk/examples/02-counter-1.0.0'),str(root/'counter.sis')],check=True,stdout=subprocess.DEVNULL)
   self.assertEqual((root/'counter.sis').read_bytes(),(root/'dist/sis-test-kit/packages/02-counter-1.0.0.sis').read_bytes())
   (root/'dist/sis-test-kit/private-note.txt').write_text('PRIVATE MUST NOT SHIP')
   (root/'dist/sis-test-kit/sources/private.lua').write_text('PRIVATE SOURCE')
   release.release(root/'dist');self.assertEqual(first,[hashlib.sha256(p.read_bytes()).hexdigest() for p in [sdk,kit]])
if __name__=='__main__':unittest.main()
