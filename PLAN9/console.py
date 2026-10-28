#!/usr/bin/env python3
"""Type into the headless Plan 9 console and dump its screen, through the QEMU
monitor socket that run.sh opens. Usage: console.py 'text\n' [screen.png]"""
import os, socket, sys, time

KEYS = {' ': 'spc', '\n': 'ret', '\t': 'tab', '/': 'slash', '.': 'dot', '-': 'minus', ',': 'comma',
        ';': 'semicolon', '=': 'equal', "'": 'apostrophe', '[': 'bracket_left', ']': 'bracket_right',
        '\\': 'backslash', '`': 'grave_accent', '!': 'shift-1', '@': 'shift-2', '#': 'shift-3',
        '$': 'shift-4', '%': 'shift-5', '^': 'shift-6', '&': 'shift-7', '*': 'shift-8', '(': 'shift-9',
        ')': 'shift-0', '_': 'shift-minus', '+': 'shift-equal', '{': 'shift-bracket_left',
        '}': 'shift-bracket_right', '|': 'shift-backslash', ':': 'shift-semicolon', '"': 'shift-apostrophe',
        '<': 'shift-comma', '>': 'shift-dot', '?': 'shift-slash', '~': 'shift-grave_accent'}
RUN = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'run')

def key(c):
    return KEYS.get(c) or ('shift-' + c.lower() if c.isupper() else c)

s = socket.socket(socket.AF_UNIX)
s.connect(os.path.join(RUN, 'monitor'))
for c in sys.argv[1] if len(sys.argv) > 1 else '':
    s.send(f'sendkey {key(c)}\n'.encode())
    time.sleep(0.03)
if len(sys.argv) > 2:
    time.sleep(2)
    ppm = os.path.join(RUN, 'screen.ppm')
    s.send(f'screendump {ppm}\n'.encode())
    time.sleep(1)
    from PIL import Image
    Image.open(ppm).save(sys.argv[2])
s.close()
