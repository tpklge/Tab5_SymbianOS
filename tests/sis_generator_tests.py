#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import importlib.util, json, pathlib, tempfile, shutil, unittest, hashlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('make_sis',ROOT/'tools/make_sis.py');sis=importlib.util.module_from_spec(spec);spec.loader.exec_module(sis)
class Generator(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name);self.src=self.root/'app';shutil.copytree(ROOT/'sdk/sis-hello',self.src);self.meta=json.loads((self.src/'sis.json').read_text())
 def tearDown(self):self.tmp.cleanup()
 def write(self): (self.src/'sis.json').write_text(json.dumps(self.meta))
 def invalid(self):
  self.write()
  with self.assertRaises((ValueError,OSError,TypeError)):sis.build(self.src,self.root/'bad.sis')
 def test_deterministic(self):
  sis.build(self.src,self.root/'a.sis');sis.build(self.src,self.root/'b.sis');a=(self.root/'a.sis').read_bytes();self.assertEqual(a,(self.root/'b.sis').read_bytes());self.assertEqual(hashlib.sha256(a).hexdigest(),'1e2a968854ae45119d0331070efc242520e7b3e6dd4271e1e33506c634b3edb1');self.assertEqual(a[:4],bytes.fromhex('7a1a2010'))
 def test_paths(self):
  for name in ['../main.lua','/etc/passwd','manifest.txt','a/b','bad.','A.t5-new']:
   self.meta['files']=['main.lua','LICENSE.txt',name];self.invalid()
 def test_alias(self): self.meta['files']+=['MAIN.LUA'];self.invalid()
 def test_uid(self):
  for uid in [False,-1,0x10201a7a,2**32,'garbage']:
   self.meta['uid']=uid;self.invalid()
 def test_version(self):
  for v in ['1','1.0','01.0.0','1.2.3.4','2147483648.0.0']:
   self.meta['version']=v;self.invalid()
 def test_links(self):
  (self.src/'main.lua').unlink();(self.src/'main.lua').symlink_to(ROOT/'sdk/sis-hello/main.lua');self.invalid()
 def test_source(self): (self.src/'main.lua').write_bytes(b'\x1bLua');self.invalid()
 def test_limit(self): (self.src/'main.lua').write_bytes(b'x'*65537);self.invalid()
 def test_reserved_id(self):self.meta["id"]="network";self.invalid()
 def test_capabilities(self):self.meta['permissions']=['network'];self.invalid()
if __name__=='__main__':unittest.main()
