"""Compatibility entry point for the Phase 1 executable C simulation suite.

The old extracted-function test assumed DIN was disabled at boot. Phase 1
replaces that behavior and moves the real controller into valve_control.c.
The previous script is retained in the pre-edit baseline source.zip.
"""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).with_name('run_phase1.py')), run_name='__main__')
