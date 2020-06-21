#!/usr/bin/env python3

from histograms import histogram, axis

# ax = axis(1,10,[6,20,50],100)
# print(ax)
# print(ax.find_bin_index(43))
# # print(ax.find_bin_index("lumberjack"))
# print(ax(33))
# # print(ax("brian"))
# # print(ax.find_bin_index(15,2))
# # print(ax(15,2))
# print(ax.nbins())
# print(ax.nedges())
#
# print(axis((5,1,5)))
# print(axis((10,0,10),5.5,11,(9,10,100),71.5))
#
# print()
#
# tup = (1,5,10,50,100)
#
# # # h = histogram()
# # h = histogram(tup)
# # h = histogram([1.,2,3,4,5])
# # # h = histogram(1,2) # should fail
# # # h = histogram([]) # should fail
# # # h = histogram([(1,1),2,3,4,5]) # should fail
# # h = histogram([(5,0,5)])
# # h = histogram([(3,1,4),5,(5,10,5)])
# # h = histogram([(3,1,4),5,(5,10,5)],tup,bintype=float)
#
# # h = histogram(tup,tup,bintype='s')

class hbin:
    def __init__(self):
        self.x = float(0)
    def __iadd__(self,x):
        self.x += x
        return self

h = histogram((1,2,3),[(5,0,10)],bintype=float)

print("size =",h.size())
print("len =",len(h))

print(h[0,])
a = h(0,0)
print(a)
h()
print(a)
print(h[0])

# print(h(2,1.5))
# print(h[15+1])

