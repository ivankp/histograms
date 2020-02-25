.PHONY: all clean install

all: build

build: setup.py histograms.cc
	@rm -rfv build
	CC=g++ python3 setup.py build

install: build
	CC=g++ python3 setup.py install

clean:
	@rm -rfv build

