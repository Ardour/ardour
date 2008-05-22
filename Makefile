all: 
	true
	#make sconsi
	#./build-tmp.sh

scons:
	scons DEBUG=1 DIST_TARGET=i686 -j 3

sconsi:
	scons --implicit-deps-unchanged DEBUG=1 DIST_TARGET=i686 -j 3

cscope: cscope.out

cscope.out: cscope.files
	cscope -b

cscope.files:
	find . -name '*.[ch]' -o -name '*.cc' > $@

.PHONY: all cscope.files sconsi cscope
