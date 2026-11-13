#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Keep the two latest versioned distribution directories; never touches SD/device."""
from pathlib import Path
import re, shutil
ROOT=Path(__file__).resolve().parents[1]
def prune(root=None):
 root=Path(root) if root is not None else ROOT/'sist'
 if not root.exists():return []
 versions=sorted((p for p in root.iterdir() if p.is_dir() and not p.is_symlink() and re.fullmatch(r'\d+\.\d+\.\d+',p.name)),key=lambda p:tuple(map(int,p.name.split('.'))),reverse=True)
 for path in versions[2:]:shutil.rmtree(path)
 return [p.name for p in versions[:2]]
if __name__=='__main__':print('Kept:',', '.join(prune()))
