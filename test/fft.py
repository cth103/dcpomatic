#!/usr/bin/python

from pylab import *
import numpy

f = open('build/test/lpf_ir')
ir = []
while True:
    l = f.readline()
    if l == "":
        break

    ir.append(float(l.strip()))

#plot(abs(numpy.fft.fft(ir)))
plot(ir)
show()
