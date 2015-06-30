#!/usr/bin/python

import matplotlib.pyplot as plt
import numpy as np

x = np.logspace(-3, 0, 100)

plt.loglog(x, pow(x, 2.2))
# plt.loglog(x, pow(x, 2.4))

srgb_linearised = []
for xx in x:
    if xx > 0.04045:
        srgb_linearised.append(pow((xx + 0.055) / 1.055, 2.4))
    else:
        srgb_linearised.append(xx / 12.92)

# plt.loglog(x, srgb_linearised)

rec_linearised = []
for xx in x:
    if xx > 0.081:
        rec_linearised.append(pow((xx + 0.099) / 1.099, 1 / 0.45))
    else:
        rec_linearised.append(xx / 4.5)

plt.loglog(x, rec_linearised)

plt.show()
