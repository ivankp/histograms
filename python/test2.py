#!/usr/bin/env python3

from histograms import histogram, axis

class hbin:
    def __init__(self):
        self.x = float(0)
    def __iadd__(self,x):
        self.x += x
        return self

h = histogram((1,2,3),[(5,0,10),11],bintype=list)

print("size =",h.size())
print("len =",len(h))

print(h[0])
print(h((0,-1),['a']))
c = (0,-1)
a = 'a'
b = 'b'
print(h.fill(c,(1.1,a)))
print(h(c,[b]))
# h()
# h(1)
# h(1,1,1)
# h(tuple())
# h(tuple(),3)
print(h[0,0])

print(c)
print(a)
print(b)

print(h.size())

print(h([1.5,4],'x','y','z'))
print(h[1,3])
print(h[(1,3)])
print(h[13])
print(h.axis(0).find_bin_index(1.5))
print(h.axis(1).find_bin_index(4))
print(h.join_index(1,3))
print(h.join_index(1,4))
print(h.join_index(2,3))
# print(h.join_index(45))
print()

h2 = histogram(((10,0,1),))
h2(0.55)
h2((0.4,),42)
for b in enumerate(h2):
    print(b)

