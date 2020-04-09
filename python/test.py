#!/usr/bin/env python3

from histograms import histogram, axis

ax = axis(1,10,[6,20,50],100)
print(ax.find_bin_index(43))
print(ax(33))
# print(ax.find_bin_index(15,2))
print(ax.nbins())
print(ax.nedges())

# # h = histogram(1,2) # should fail
# # h = histogram([]) # should fail
# # h = histogram([(1,1),2,3,4,5]) # should fail
# # h = histogram([1.,2,3,4,5])
# # h = histogram([5,(0,5)])
# h = histogram([1.,2,3,4,5],[5,(0,5)],[0])
# # h = histogram()
#
# print("nbins =",h.nbins())
# print("len =",len(h))
#
# h()

