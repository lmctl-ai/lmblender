#!/usr/bin/env python3
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('flags', ROOT/'tools/check-lmcode-host-flags.py')
flags = importlib.util.module_from_spec(spec); spec.loader.exec_module(flags)
class Gate(unittest.TestCase):
    def test_rejects_options_and_only_allows_exact_historical_lines(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            subprocess.run(['git','init','-q',tmp],check=True)
            line = b'host ' + flags.OLD + b'-schema --json'
            (root/'README.md').write_bytes(line+b'\n')
            subprocess.run(['git','-C',tmp,'add','README.md'],check=True)
            self.assertEqual(flags.scan(root), (['README.md:1'],0))
            (root/'.lmcode-flag-history.json').write_text(json.dumps({'lines':{'README.md':[hashlib.sha256(line).hexdigest()]}}))
            self.assertEqual(flags.scan(root), ([],1))
            (root/'README.md').write_bytes(line+b'\n'+line+b'\n')
            self.assertEqual(flags.scan(root), (['README.md:2'],1))
            (root/'README.md').write_bytes(b'host --lmcode-schema --json\n')
            self.assertEqual(flags.scan(root), ([],0))
if __name__ == '__main__': unittest.main()
