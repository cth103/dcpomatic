#!/usr/bin/python

import matplotlib.pyplot as plt
import numpy as np

x = np.logspace(-3, 0, 100)

plt.loglog(x, pow(x, 2.2))
plt.loglog(x, pow(x, 2.4))

linearised = []
for xx in x:
    if xx > 0.04045:
        linearised.append(pow((xx + 0.055) / 1.055, 2.4))
    else:
        linearised.append(xx / 12.92)

plt.loglog(x, linearised)
plt.show()
