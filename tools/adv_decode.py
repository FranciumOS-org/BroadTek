#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Decode broadtek-adv-cmds / broadtek-adv-status from the registry. Read-only.
#   python3 tools/adv_decode.py
import re, subprocess

OWN = {0: 'public', 1: 'random', 2: 'RPA-or-public (controller)', 3: 'RPA-or-random (controller)'}


def addr(b):
    a = ':'.join('%02x' % x for x in reversed(b))
    kind = {0x40: 'RPA', 0xc0: 'static', 0x00: 'non-resolvable'}.get(b[5] & 0xc0, '?')
    return '%s (%s)' % (a, kind)


def props(v):
    names = ['connectable', 'scannable', 'directed', 'hi-duty', 'legacy', 'anonymous', 'txpower']
    return '|'.join(n for i, n in enumerate(names) if v & (1 << i)) or 'none'


reg = subprocess.run(['ioreg', '-l', '-w0'], capture_output=True, text=True).stdout
cmds = re.search(r'"broadtek-adv-cmds" = "([^"]*)"', reg)
stat = re.search(r'"broadtek-adv-status" = "([^"]*)"', reg)
if not cmds:
    raise SystemExit('no broadtek-adv-cmds: build 978c1ac or later not loaded, or nothing sent yet')
for item in cmds.group(1).split():
    head, hexs = item.split(':', 1)
    op, rest = head.split('/')
    handle, count = rest.split('#')
    b = bytes.fromhex(hexs)
    op = int(op, 16)
    line = '%04x set-key %s x%s' % (op, handle, count)
    if op == 0x2005 and len(b) >= 6:
        line += '  LE Set Random Address ' + addr(b[:6])
    elif op == 0x2035 and len(b) >= 7:
        line += '  LE Set Adv Set Random Address set %d %s' % (b[0], addr(b[1:7]))
    elif op == 0x2036 and len(b) >= 25:
        pmin = b[3] | b[4] << 8 | b[5] << 16
        line += ('  LE Set Ext Adv Params set %d props %s interval %.2f ms own %s peer %d/%s '
                 'chmap %x sid %d phy %d/%d' % (b[0], props(b[1] | b[2] << 8), pmin * 0.625,
                                                 OWN.get(b[10], b[10]), b[11], addr(b[12:18]),
                                                 b[9], b[23], b[20], b[22]))
    elif op == 0x2039 and len(b) >= 2:
        line += '  LE Set Ext Adv Enable %s sets %d' % ('on' if b[0] else 'off', b[1])
    elif op == 0x2006 and len(b) >= 15:
        line += '  LE Set Adv Params (legacy) type %d own %s' % (b[4], OWN.get(b[5], b[5]))
    else:
        line += '  ' + hexs
    print(line)
print('status:', stat.group(1) if stat else '(none)')
