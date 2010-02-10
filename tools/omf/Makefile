CXXFLAGS = -g -I../../libs/pbd \
           $(shell pkg-config --cflags sqlite3) \
           $(shell pkg-config --cflags libxml-2.0) \
           $(shell pkg-config --cflags glibmm-2.4) \
           $(shell pkg-config --cflags sndfile)

LDFLAGS = -L../../build/default/libs/pbd -lpbd  \
	  $(shell pkg-config --libs sqlite3) \
	  $(shell pkg-config --libs libxml-2.0) \
	  $(shell pkg-config --libs sndfile) \

omftool: omftool.o loader.o
	$(CXX) -o $@ omftool.o loader.o $(LDFLAGS)

loader.o:  loader.cc omftool.h
omftool.o: omftool.cc omftool.h
