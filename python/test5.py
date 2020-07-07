#!/usr/bin/env python3

from histograms import histogram, mc_bin

for T in (float,int,mc_bin):
    print(T.__name__)
    h = histogram(((10,0,1),),bintype=T)

    print(h(0.15))
    print(h(0.16))
    print(h(0.17))
    print(h(0.18))
