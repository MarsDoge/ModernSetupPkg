#!/usr/bin/env python3
"""Exercise build identity generation without touching the live package header."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class BuildStampTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'Scripts').mkdir()
        (self.root / 'Application/ModernSetupApp').mkdir(parents=True)
        self.script = self.root / 'Scripts/generate-build-stamp.py'
        shutil.copyfile(ROOT / 'Scripts/generate-build-stamp.py', self.script)
        self.header = self.root / 'Application/ModernSetupApp/ModernSetupBuildStamp.generated.h'

    def generate(self, epoch):
        env = dict(os.environ, SOURCE_DATE_EPOCH=epoch, TZ='UTC')
        return subprocess.run([sys.executable, str(self.script)], env=env,
                              text=True, capture_output=True)

    def test_explicit_epoch_uses_shanghai_not_host_timezone(self):
        result = self.generate('0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), '1970-01-01 08:00:00 +08:00')
        self.assertIn('L"Build: 1970-01-01 08:00:00 +08:00"', self.header.read_text())

    def test_repeated_generation_preserves_content_and_mtime(self):
        self.assertEqual(self.generate('0').returncode, 0)
        before = self.header.read_bytes(), self.header.stat().st_mtime_ns
        self.assertEqual(self.generate('0').returncode, 0)
        self.assertEqual(before, (self.header.read_bytes(), self.header.stat().st_mtime_ns))

    def test_invalid_override_does_not_replace_previous_header(self):
        self.assertEqual(self.generate('0').returncode, 0)
        before = self.header.read_bytes()
        self.assertNotEqual(self.generate('not-an-epoch').returncode, 0)
        self.assertEqual(before, self.header.read_bytes())

    def test_new_epoch_changes_frozen_identity(self):
        self.assertEqual(self.generate('0').returncode, 0)
        self.assertEqual(self.generate('1').returncode, 0)
        self.assertIn('08:00:01 +08:00', self.header.read_text())


if __name__ == '__main__':
    unittest.main()
