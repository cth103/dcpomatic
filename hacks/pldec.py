#!/usr/bin/python

import numpy as np
import scipy.signal
import matplotlib.pylab as plt
import math

radians = np.pi * 16
points = 1024
cut = math.pow(10, -3 / 20.0)
print cut

t = np.linspace(0, radians, points)
inL = np.zeros((points,))
inC = np.sin(t)
inR = np.zeros((points,))
inS = np.zeros((points,))

# Encode
Lt = inL + inC * cut + np.imag(scipy.signal.hilbert(inS * cut))
Rt = inR + inC * cut - np.imag(scipy.signal.hilbert(inS * cut))

# Decode
outL = Lt
outR = Rt
outS = Lt - Rt

plt.plot(t, outL, label='L')
plt.plot(t, outR, label='R')
plt.plot(t, outS, label='S')
plt.legend()
plt.show()

