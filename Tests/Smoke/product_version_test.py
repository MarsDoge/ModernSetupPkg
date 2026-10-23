#!/usr/bin/env python3
"""Keep product metadata, visible version and release notes synchronized."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
version = re.search(r'MODERN_SETUP_VERSION_STRING\s+L"(\d+\.\d+\.\d+)"',
                    (ROOT / 'Include/ModernUi/ModernUiVersion.h').read_text()).group(1)
for name, key in [('ModernSetupPkg.dec', 'PACKAGE_VERSION'),
                  ('Application/ModernSetupApp/ModernSetupApp.inf', 'VERSION_STRING')]:
    actual = re.search(r'^\s*' + key + r'\s*=\s*(\S+)',
                       (ROOT / name).read_text(), re.M).group(1)
    assert actual == version, (name, actual, version)
assert f'## v{version} - ' in (ROOT / 'CHANGELOG.md').read_text()
print(f'PASS product version {version}: UI, package, App and changelog agree')
