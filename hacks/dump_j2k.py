#!/usr/bin/python

import sys

def read(f, n):
    r = []
    for i in range(0, n):
        r.append(ord(f.read(1)))
    return r

def read_8(f):
    r = read(f, 1)
    return r[0]

def read_16(f):
    r = read(f, 2)
    return r[0] << 8 | r[1]

def read_32(f):
    r = read(f, 4)
    return r[0] << 24 | r[1] << 16 | r[2] << 8 | r[3];

def require(f, data, what):
    r = read(f, len(data))
    if r != data:
        print r
        raise Exception()
    print what

f = open(sys.argv[1], 'rb')

require(f, [0xff, 0x4f], 'SOC')
require(f, [0xff, 0x51], 'SIZ')
size = read_16(f)
print '\tlength', size
f.seek(size - 2, 1)
require(f, [0xff, 0x52], 'COD')
size = read_16(f)
print '\tlength', size
coding_style = read_8(f)
print '\tcoding style %2x' % coding_style
print '\tprogression order',
po = read_8(f)
if po == 0:
    print 'LRCP'
elif po == 1:
    print 'RLCP'
elif po == 2:
    print 'RPCL'
elif po == 3:
    print 'PCRL'
elif po == 4:
    print 'CPRL'
print '\tlayers', read_16(f)
print '\tmulti-component transform', read_8(f)
levels = read_8(f)
print '\ttransform levels', levels
print '\tcode-block sizes %dx%d' % (read_8(f), read_8(f))
print '\tmode switches %2x' % read_8(f)
print '\twavelet transform',
wt = read_8(f)
if wt == 0:
    print '9/7 irreversible'
else:
    print '5/3 reversible'
if coding_style & 1:
    print '\tprecinct sizes ',
    for i in range(0, levels + 1):
        print read_8(f),
    print
require(f, [0xff, 0x5C], 'QCD')
size = read_16(f)
print '\tlength', size
f.seek(size - 2, 1)

tile_part_length = None

while True:
    r = read(f, 2)
    if r == [0xff, 0x53]:
        print 'COC'
        size = read_16(f)
        print '\tlength', size
        f.seek(size - 2, 1)
    elif r == [0xff, 0x5c]:
        print 'QCD'
        size = read_16(f)
        print '\tlength', size
        f.seek(size - 2, 1)
    elif r == [0xff, 0x5d]:
        print 'QCC'
        size = read_16(f)
        print '\tlength', size
        f.seek(size - 2, 1)
    elif r == [0xff, 0x64]:
        print 'COM'
        size = read_16(f)
        print '\tlength', size
        f.seek(size - 2, 1)
    elif r == [0xff, 0x55]:
        print 'TLM'
        size = read_16(f)
        print '\tlength', size
        f.seek(size - 2, 1)
    elif r == [0xff, 0x90]:
        print 'SOT'
        size = read_16(f)
        print '\tlength', size
        print '\ttile index', read_16(f)
        tile_part_length = read_32(f)
        print '\ttile-part length', tile_part_length
        print '\ttile-part index', read_8(f)
        print '\tnumber of tile-parts', read_8(f)
    elif r == [0xff, 0x93]:
        print 'SOD'
        f.seek(tile_part_length - 14, 1)
    elif r == [0xff, 0xd9]:
        print 'EOC'
        sys.exit(0)
    else:
        print r
        raise Exception()
