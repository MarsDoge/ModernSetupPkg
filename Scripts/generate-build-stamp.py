#!/usr/bin/env python3
"""Freeze the App build identity; SOURCE_DATE_EPOCH overrides wall time."""
import datetime
import os
from pathlib import Path
import time
from zoneinfo import ZoneInfo


def build_stamp():
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH', str(int(time.time()))))
    return datetime.datetime.fromtimestamp(epoch, ZoneInfo('Asia/Shanghai')).strftime('%Y-%m-%d %H:%M:%S +08:00')


if __name__ == '__main__':
    stamp = build_stamp()
    target = Path(__file__).resolve().parents[1] / 'Application/ModernSetupApp/ModernSetupBuildStamp.generated.h'
    content = '// Generated at build time; override with SOURCE_DATE_EPOCH.\n#define MODERN_SETUP_BUILD_STAMP L"Build: ' + stamp + '"\n'
    if not target.exists() or target.read_text() != content:
        target.write_text(content)
    print(stamp)
