#!/usr/bin/env python3

from histograms import histogram as hist, axis
from copy import copy, deepcopy

h = hist(((10,-5,5),))
h(1)
h(2)
h(2.5)
print(*h)

print( copy(h.axis(0)) )
print( deepcopy(h.axis(0)) )
print( axis(copy(h.axis(0))) )
print()

h2 = hist(h)
print( h2.axis(0) )
print( *h2 )
print()

h2 = copy(h)
print( h2.axis(0) )
print( *h2 )
print()

h2 = deepcopy(h)
print( h2.axis(0) )
print( *h2 )
print()

# import pickle

# pickle.dump(h.axis(0),open("test.p","wb"))
# print( pickle.load(open("test.p","rb")) )

# pickle.dump(h,open("test.p","wb"))
# print( pickle.load(open("test.p","rb")) )

