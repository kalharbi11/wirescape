"""Finds the compiled surgepy module and puts it on sys.path.

Search order: $SURGEPY_DIR, ./surgepy_bin (where build_windows.bat copies it), then the default
build folders inside ./surge.
"""
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
_candidates = [
    os.environ.get("SURGEPY_DIR", ""),
    os.path.join(HERE, "surgepy_bin"),
    *glob.glob(os.path.join(HERE, "surge", "ignore", "bpy", "src", "surge-python", "*")),
    os.path.join(HERE, "surge", "ignore", "bpy", "src", "surge-python"),
]
for c in _candidates:
    if c and os.path.isdir(c) and (glob.glob(os.path.join(c, "surgepy*.pyd"))
                                    or glob.glob(os.path.join(c, "surgepy*.so"))):
        if c not in sys.path:
            sys.path.insert(0, c)
        break
