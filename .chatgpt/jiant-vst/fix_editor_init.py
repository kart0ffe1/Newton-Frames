#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: fix_editor_init.py PATH/TO/Plugin.cpp")

p = Path(sys.argv[1])
s = p.read_text(encoding="utf-8")

old = '''        setLookAndFeel (&look);\n        setSize (1040, 650);\n        setResizable (true, true);\n        setResizeLimits (850, 560, 1500, 950);\n\n        for (int i = 0; i < kKnobCount; ++i)\n'''
new = '''        setLookAndFeel (&look);\n\n        // Build all child components before setting the editor size. JUCE may call\n        // resized() synchronously from setSize(), and resized() indexes these arrays.\n        for (int i = 0; i < kKnobCount; ++i)\n'''

if old not in s:
    raise SystemExit("editor constructor preamble changed")
s = s.replace(old, new, 1)

old2 = '''        startTimerHz (30);\n    }\n'''
new2 = '''        // Only now is resized() safe: knobs/buttons/labels all exist.\n        setResizable (true, true);\n        setResizeLimits (850, 560, 1500, 950);\n        setSize (1040, 650);\n        startTimerHz (30);\n    }\n'''

if old2 not in s:
    raise SystemExit("editor constructor timer tail changed")
s = s.replace(old2, new2, 1)

p.write_text(s, encoding="utf-8")
print(f"patched editor init order in {p}")
