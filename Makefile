all: waf

waf:
	./waf
cscope: cscope.out

cscope.out: cscope.files
	cscope -b

cscope.files:
	find . -name '*.[ch]' -o -name '*.cc' > $@

install uninstall:
	./waf $@

.PHONY: all cscope.files sconsi cscope waf install
