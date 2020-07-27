#!/usr/bin/env python3

from histograms import histogram, axis

h = histogram((1,2,3),[(5,0,10),11])

# print(list(h))

for a in h.axes():
    print(a)

print(h.ndim())
print(h.axis(0))
print(h.axis(1))
print(h.axis(-1))
print(h.axis(-2))

print(*h.axis(0))
print(*h.axis(1))

print(h.axis(0).find_bin_index(15.1))
print()

print(h.join_index(1,1))
print(h.join_index([1,1]))
print(h.join_index((1,1)))
print(h.join_index(i for i in [1,1]))
h(1.5,3)
print(*h)
print(h.find_bin_index(1.5,3))
print(h.find_bin_index([1.5,3]))
print(h.find_bin_index((1.5,3)))
print(h.find_bin_index(x for x in [1.5,3]))
print()

h2 = histogram(h)
print(h2[9])
h3 = histogram(h,bintype=float)
print(h3[9])
# h4 = h.copy()
# h5 = h.deepcopy()
