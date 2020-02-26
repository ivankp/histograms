#!/usr/bin/env python3

from histograms import histogram

# h = histogram(1,2) # should fail
# h = histogram([(1,1),2,3,4,5]) # should fail
h = histogram([1.,2,3,4,5])
# h = histogram([5,(0,5)])
# h = histogram([1.,2,3,4,5],[5,0,5],[0])

print("nbins =",h.nbins())

