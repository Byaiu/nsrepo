# Makefile wrapper for waf

all:
	./waf

# free free to change this part to suit your requirements
configure:
	./waf configure --enable-examples --enable-tests

config-debug:
	./waf configure --build-profile=debug --out=build/debug
build-debug: config-debug
	./waf 

config-opt:
	./waf configure --build-profile=optimized --out=build/optimized 
build-opt: config-opt
	./waf 

build:
	./waf build

install:
	./waf install

clean:
	./waf clean

distclean:
	./waf distclean
