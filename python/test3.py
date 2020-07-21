#!/usr/bin/env python3

from histograms import histogram, axis

h = histogram((1,2,3),[(5,0,10),11])

# print(list(h))

for a in h.axes():
    print(a)

print(h.naxes())
print(h.axis(0))
print(h.axis(1))
print(h.axis(-1))
print(h.axis(-2))

print(h.axis(0).find_bin_index(15.1))

h(1.5,3)
print(*h)
# print(h.fin_bin_index(1.5,3))
h2 = histogram(h)
# h2 = histogram(h,bintype=float)
print(h2[9])
# h3 = h.copy()
# h4 = h.deepcopy()
