#!/usr/bin/env python3

from histograms import histogram, axis

h = histogram((1,2,3),[(5,0,10),11],bintype=list)

for a in h.axes():
    print(a)

print(h.naxes())
print(h.axis(0))
print(h.axis(1))
print(h.axis(-1))
print(h.axis(-2))

print(list(h))
