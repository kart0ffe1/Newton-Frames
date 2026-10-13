#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: fix_compile.py PATH/TO/Plugin.cpp")

p = Path(sys.argv[1])
s = p.read_text(encoding="utf-8")
old = "tempBackendFile = {}; currentFirmwareId.clear(); resetAudioClockLocked();"
new = "tempBackendFile = juce::File(); currentFirmwareId.clear(); resetAudioClockLocked();"
if old not in s:
    raise SystemExit("expected JUCE File reset not found")
p.write_text(s.replace(old, new, 1), encoding="utf-8")
print(f"patched JUCE File reset in {p}")
