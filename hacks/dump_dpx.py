#!/usr/bin/python3

import sys
import struct

f = open(sys.argv[1], 'rb')

dpx = f.read(768)
magic = dpx[0:4]
if magic == b'XPDS':
    print('Little-endian')
    endian = '<'
elif magic == b'SDPX':
    print('Big-endian')
    endian = '>'
else:
    print('Unrecognised magic word', file=sys.stderr)
    sys.exit(1)

image = f.read(640)
im = dict()
(im['orientation'],
 im['number_elements'],
 im['pixels_per_line'],
 im['lines_per_element'],
 im['data_sign'],
 im['low_data'],
 im['low_quantity'],
 im['high_data'],
 im['high_quantity'],
 im['descriptor'],
 im['transfer'],
 im['colorimetric']) = struct.unpack('%shhiiiififBBB' % endian, image[0:35])

transfer = { 0: 'user-defined', 1: 'printing density', 2: 'linear', 3: 'logarithmic', 4: 'unspecified video', 5: 'SMPTE 240M', 6: 'CCIR 709-1', 7: 'CCIR601-2 system B or G',
             8: 'CCIR 601-2 system M', 9: 'NTSC composite video', 10: 'PAL composite video', 11: 'Z linear', 12: 'Z homogeneous' }

for k, v in im.items():
    if k == 'transfer':
        v = transfer[v]
    print('%s: %s' % (k, v))
