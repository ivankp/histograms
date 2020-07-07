#!/usr/bin/env python3

from histograms import histogram

for T in (float,int):
    print(T.__name__)
    h = histogram(((10,0,1),),bintype=T)

    print(h(0.15))
    print(h(0.16))
    print(h(0.17))
    print(h(0.18))
