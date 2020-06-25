#!/usr/bin/env python3

from histograms import histogram, axis
import random

h = histogram(((100,-1,1),),bintype=int)

for i in range(100000):
    h(random.gauss(0,0.4))

for b in h:
    print(b)

# import numpy as np
# bins = np.fromiter(h,float)
# print(bins)
