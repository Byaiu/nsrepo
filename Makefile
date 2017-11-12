# Makefile wrapper for waf


config-debug:
	./waf configure --build-profile=debug --out=build/debug
build-debug: config-debug
	./waf 

config-opt:
	./waf configure --build-profile=optimized --out=build/optimized 
build-opt: config-opt
	./waf 


clean:
	./waf clean

distclean:
	./waf distclean
