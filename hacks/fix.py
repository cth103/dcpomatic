#!/usr/bin/python3

import os
import re
import sys

filename = sys.argv[1]

o = open(filename + '.tmp', 'w')

for l in open(sys.argv[1]).readlines():
    if l.find("test/data") != -1 or l.find("build/test") != -1:
        m = re.match('.*("[^"]*")', l)
        try:
            path = m.group(1)
            bits = path[1:-1].split('/')
            fixed = 'path("%s")' % bits[0]
            for b in bits[1:]:
                fixed += ' / "%s"' % b
            l = l.replace(path, fixed)
        except:
            pass
    print(l, end='', file=o)

os.rename(filename + '.tmp', filename)


