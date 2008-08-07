all: scons

scons:
	scons

sconsi:
	scons --implicit-deps-unchanged

cscope: cscope.out

cscope.out: cscope.files
	cscope -b

cscope.files:
	find . -name '*.[ch]' -o -name '*.cc' > $@

.PHONY: all cscope.files sconsi cscope
