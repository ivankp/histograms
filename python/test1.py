#!/usr/bin/env python3

from histograms import histogram, axis

ax = axis(1,10,[6,20,50],100)
print(ax)
print(ax.find_bin_index(43))
# print(ax.find_bin_index("lumberjack"))
print(ax(33))
# print(ax("brian"))
# print(ax.find_bin_index(15,2))
# print(ax(15,2))
print(ax.nbins())
print(ax.nedges())

print(axis((5,1,5)))
print(axis((10,0,10),5.5,11,(9,10,100),71.5))
print()

a1 = axis((5,0,100))
print(a1)
a2 = axis(*a1)
print(a2)
print()

tup = (1,5,10,50,100)

# h = histogram()
h = histogram(tup)
h = histogram([1.,2,3,4,5])
# h = histogram(1,2) # should fail
# h = histogram([]) # should fail
# h = histogram([(1,1),2,3,4,5]) # should fail
h = histogram([(5,0,5)])
h = histogram([(3,1,4),5,(5,10,5)])
h = histogram([(3,1,4),5,(5,10,5)],tup,bintype=float)

