#!/usr/bin/python

from PIL import Image
import numpy
from libtiff import TIFF

width = 1998
height = 1080
filename = 'test.tif'

im = numpy.zeros((height, width, 3), dtype=numpy.uint16)

# Convert 12 to 16-bit
def pixel(x):
    return x << 4

# Bars of increasing intensity in X
for x in range(0, width):
    for y in range(0, height):
        if x < 400:
            im[y][x][0] = pixel(0)
        elif x < 800:
            im[y][x][0] = pixel(1024)
        elif x < 1200:
            im[y][x][0] = pixel(2048)
        elif x < 1600:
            im[y][x][0] = pixel(3072)
        else:
            im[y][x][0] = pixel(4095)

# Ramp in Y
for x in range(0, width):
    for y in range(0, height):
        im[y][x][1] = pixel((x * 4) % 4096)

tiff = TIFF.open(filename, mode='w')
tiff.write_image(im, write_rgb=True)
tiff.close()
