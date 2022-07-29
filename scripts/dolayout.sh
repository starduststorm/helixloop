#!/bin/sh
/Applications/Kicad/kicad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 board/layout.py -p board/helixloop.kicad_pcb --skip-traces && open board/helixloop.kicad_pcb

