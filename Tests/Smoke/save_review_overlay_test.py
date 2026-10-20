#!/usr/bin/env python3
"""Exercise real pinned browser replacement in all platform overlays."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SDK = Path(os.environ.get('SAVE_REVIEW_EDK2', str(ROOT / 'External/edk2')))
OLD = 'MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf'
NEW = 'Build/ModernSetupPkgOverlay/SetupBrowserReview/SetupBrowserDxe.inf'
CASES = (
    ('armvirt', 'ArmVirtQemuModernSetup'),
    ('loongarchvirt', 'LoongArchVirtQemuModernSetup'),
    ('ovmf-x64', 'OvmfX64ModernSetup'),
    ('riscvvirt', 'RiscVVirtQemuModernSetup'),
)

class OverlayTests(unittest.TestCase):
    def test_pinned_production_and_optout(self):
        self.assertTrue((SDK / 'MdeModulePkg/Universal/SetupBrowserDxe/Setup.c').is_file(),
                        'Initialize External/edk2 or set SAVE_REVIEW_EDK2; this test must not silently skip')
        with tempfile.TemporaryDirectory(prefix='modern-review-overlays-') as td:
            ws = Path(td)
            for p in SDK.iterdir():
                if p.name not in ('.git', 'Build', 'Conf', 'ModernSetupPkg'):
                    (ws / p.name).symlink_to(p.resolve(), target_is_directory=p.is_dir())
            pkg = ws / 'ModernSetupPkg'
            shutil.copytree(ROOT, pkg, ignore=shutil.ignore_patterns('.git', 'External', 'Build', '__pycache__'))
            env = dict(os.environ, WORKSPACE=str(ws), SOURCE_DATE_EPOCH='0', GENERATE_ONLY='1',
                       MODERN_SETUP_REPLACE_UIAPP='0', MODERN_SETUP_DEMO_DRIVER_SAMPLE='0')
            for script, stem in CASES:
                for engine, enabled in (('modern', '1'), ('lvgl', '1'), ('native', '1'), ('lvgl', '0')):
                    with self.subTest(platform=script, engine=engine, enabled=enabled):
                        env.update(MODERN_SETUP_DISPLAY_ENGINE=engine, MODERN_SETUP_SAVE_REVIEW=enabled)
                        result = subprocess.run(['bash', str(pkg / f'Scripts/build-{script}.sh')],
                                                env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                        self.assertEqual(result.returncode, 0, result.stdout)
                        overlay = ws / 'Build/ModernSetupPkgOverlay'
                        text = (overlay / (stem + '.dsc')).read_text() + (overlay / (stem + '.fdf')).read_text()
                        if script == 'armvirt':
                            text += (overlay / 'ArmVirtQemuModernSetupFvMain.fdf.inc').read_text()
                        active = engine != 'native' and enabled == '1'
                        self.assertEqual(text.count(NEW), 2 if active else 0)
                        self.assertEqual(text.count(OLD), 0 if active else 2)
                        if active:
                            generated = overlay / 'SetupBrowserReview'
                            setup = (generated / 'Setup.c').read_text()
                            self.assertIn('ModernReviewSubmit', setup)
                            self.assertTrue((generated / 'ReviewRuntime.inc').is_file())
            env['MODERN_SETUP_SAVE_REVIEW'] = 'invalid'
            result = subprocess.run(['python3', str(pkg / 'Scripts/SaveReview/wire.py'), str(ws), 'lvgl'],
                                    env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertNotEqual(result.returncode, 0)

if __name__ == '__main__':
    unittest.main()
