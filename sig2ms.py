#!/usr/bin/python

import sys
import random
NC, C = '10'

def noise_one(c):
    if random.uniform(0., 1.) > .65: return "01"[not int(c)]
    return c

def noise(s):
    return "".join(noise_one(c) for c in s)

wf = {}
wf['0'] = NC * 200 + C * 800
wf['1'] = NC * 500 + C * 500
wf['2'] = NC * 800 + C * 200

data = []
for line in sys.stdin:
    if not line.startswith("'"): continue
    line = line.strip().split(None, 2)[2]
    data.append(line)
data = '012' * 20 + "".join(data) + '012'
try:
    for c in data:
        sys.stdout.write(noise(wf[c]))
    sys.stdout.flush()
except (IOError, KeyboardInterrupt): pass
