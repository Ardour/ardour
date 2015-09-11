all:
	g++ -o gcc5stc++test.`gcc -print-multiarch` test.cc -Wall -std=c++11 -O0
	nm gcc5stc++test.`gcc -print-multiarch` | c++filt | grep -q __cxx11
	strip gcc5stc++test.`gcc -print-multiarch`

.PHONY: all
