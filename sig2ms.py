#!/usr/bin/python

# Copyright (C) 2011 Jeff Epler <jepler@unpythonic.net>
# SPDX-FileCopyrightText: 2011 Jeff Epler
#
# SPDX-License-Identifier: GPL-3.0-only

#define __STDC_CONSTANT_MACROS 1
#include <stdint.h>
#include <string.h>
#include <stdio.h>

import sys
import random
import getopt
import numpy

opts, args = getopt.getopt(sys.argv[1:], "ij:t:n:p:")

invert = False
jitter = 0
timebase = 1000
noise = 0
phase = 0

T = { '0': .2, '1': .5, '2': .8 }

for k, v in opts:
    if k == '-i': invert = not invert
    if k == '-j': jitter = float(v)
    if k == '-t': timebase = float(v)
    if k == '-n': noise = float(v)
    if k == '-p': phase = float(v)

NC, C = '0', '1'
if invert: C, NC = NC, C

def add_noise(s):
    if not noise: return s
    s = numpy.fromstring(s, numpy.int8)
    r = numpy.random.random(size=(len(s),))
    return (s ^ (r < noise)).tostring()

t = phase - int(phase) - 1

try:
    for line in sys.stdin:
	if not line.startswith("'"): continue
	line = line.strip().split(None, 2)[2]
	for c in line:
	    t1 = random.uniform(-jitter, jitter)
	    t2 = T[c] + random.uniform(-jitter, jitter)
	    dt1 = int((t1 - t) * timebase)
	    t = t + dt1 * 1. / timebase
	    sys.stdout.write(add_noise(C * dt1))
	    dt2 = int((t2 - t) * timebase)
	    sys.stdout.write(add_noise(NC * dt2))
	    t = t + dt2 * 1. / timebase
	    t = t - int(t) - 1
    sys.stdout.write(add_noise(NC * int(timebase)) + "\n")

except (IOError, KeyboardInterrupt): pass
