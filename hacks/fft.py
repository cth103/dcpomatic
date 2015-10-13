#!/usr/bin/python

import wave
import sys
import struct
from pylab import *
import numpy

f = wave.open(sys.argv[1], 'rb')
width = f.getsampwidth()
peak = pow(2, width * 8) / 2
channels = f.getnchannels()
frames = f.getnframes()
print '%d bytes per sample' % width
print '%d channels' % channels
print '%d frames' % frames
data = []
for i in range(0, channels):
    data.append([])

for i in range(0, frames):
    frame = f.readframes(1)
    for j in range(0, channels):
        v = 0
        for k in range(0, width):
            v |= ord(frame[j * width + k]) << (k * 8)
        if v >= peak:
            v = v - 2 * peak
        data[j].append(v)

f.close()

names = ['L', 'R', 'C', 'Lfe', 'Ls', 'Rs']
dyn_range = 20 * log10(pow(2, 23))

for i in range(0, channels):
    s = numpy.fft.fft(data[i])
#    subplot(channels, 1, i + 1)
    # 138.5 is a fudge factor:
    semilogx(arange(0, frames / 2, 0.5), 20 * log10(abs(s) + 0.1) - dyn_range, label = names[i])
    xlim(100, frames / 4)
plt.gca().xaxis.grid(True, which='major')
plt.gca().xaxis.grid(True, which='minor')
plt.gca().yaxis.grid(True)
legend(loc = 'lower right')
show()

#plot(abs(numpy.fft.fft(data[0])))
#plot(data[0])
#show()
