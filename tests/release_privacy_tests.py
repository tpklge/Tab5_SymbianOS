# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import tempfile
import unittest
import json
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('tab5_release', ROOT/'tools/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)

class PrivacyTests(unittest.TestCase):
    def test_private_files_outside_system_resources_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix='tab5-release-privacy-') as temp:
            sd = Path(temp)
            for name in ('data/notes/document.txt', 'data/organizer/contacts.tsv',
                         'data/mesh/channels.tsv', 'backups/full.t5bak',
                         'logs/mail.txt', 'config/public/profile',
                         'meta/sequence.txt', 'apps/registry.tsv',
                         'tmp/leftover', 'update/runtime.bin', '.gitkeep'):
                path = sd/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'synthetic private data')
                with self.subTest(name=name), self.assertRaises(ValueError):
                    release.validate_template(sd)
                path.unlink()

    def test_distribution_metadata_and_empty_placeholders_are_allowed(self):
        with tempfile.TemporaryDirectory(prefix='tab5-release-privacy-') as temp:
            sd = Path(temp)
            for name in ('boot/manifest.txt', 'system/releases/1.0.0/shell.lua',
                         'data/notes/.gitkeep', '.DS_Store'):
                path = sd/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'' if path.name=='.gitkeep' else b'template')
            release.validate_template(sd)

    def test_links_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix='tab5-release-privacy-') as temp:
            sd = Path(temp)
            (sd/'link').symlink_to('/tmp')
            with self.assertRaises(ValueError):
                release.validate_template(sd)

    def test_metadata_is_absent_from_both_manifest_and_archive(self):
        with tempfile.TemporaryDirectory(prefix='tab5-release-metadata-') as temp:
            sd = Path(temp)/'SymbianOS'
            resources = sd/'system/releases/1.0.0'
            resources.mkdir(parents=True)
            (sd/'boot').mkdir()
            apps = {'shell': 'shell.lua', 'notes': 'notes.lua', 'calculator': 'calculator.lua'}
            for name in list(apps.values()) + ['theme.ttf', 'chrome.lua', 'keyboard.map', 'onscreen.keys', 'store.lua', '.DS_Store', '._shell.lua']:
                (resources/name).write_bytes(b'synthetic resource')
            (resources/'release.json').write_text(json.dumps({'apps': apps, 'grants': {}, 'capabilities': {}}))
            archive = Path(temp)/'sd.zip'
            subprocess.run([sys.executable, '-B', str(ROOT/'tools/package_sd.py'), '--root', str(sd), '--zip', str(archive)], check=True, capture_output=True)
            manifest = (sd/'boot/manifest.txt').read_text()
            self.assertNotIn('.DS_Store', manifest)
            self.assertNotIn('._shell.lua', manifest)
            with zipfile.ZipFile(archive) as package:
                self.assertFalse(any(Path(name).name in ('.DS_Store', '._shell.lua') for name in package.namelist()))

if __name__ == '__main__':
    unittest.main()
