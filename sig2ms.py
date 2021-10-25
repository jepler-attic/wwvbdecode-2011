#!/usr/bin/python
# Copyright (C) 2011 Jeff Epler <jepler@unpythonic.net>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#define __STDC_CONSTANT_MACROS 1
#include <stdint.h>
#include <string.h>
#include <stdio.h>

import sys
import random
NC, C = '10'

def noise_one(c):
    if random.uniform(0., 1.) > .65: return "01"[not int(c)]
    return c

def noise(s):
    return s
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
