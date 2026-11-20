#!/usr/bin/env python3
"""Writes a Synthia preset/state file from id=value pairs (others = defaults).
   python3 scripts/mkpreset.py out.synthia vcOn=0 rvMix=1 rvMode=1
Values are real parameter values (choice = index, bool = 0/1). Handy with synthia-render."""
import sys
from xml.sax.saxutils import quoteattr
out = sys.argv[1]
pairs = [a.split('=', 1) for a in sys.argv[2:]]
with open(out, 'w') as f:
    f.write('<?xml version="1.0" encoding="UTF-8"?>\n<SynthiaState stateVersion="1" presetName=%s>\n  <PARAMS>\n' % quoteattr(out))
    for k, v in pairs:
        f.write('    <PARAM id=%s value=%s/>\n' % (quoteattr(k), quoteattr(v)))
    f.write('  </PARAMS>\n</SynthiaState>\n')
