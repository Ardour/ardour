CXXFLAGS = -g -I../../libs/pbd \
           $(shell pkg-config --cflags sqlite3) \
           $(shell pkg-config --cflags libxml-2.0) \
           $(shell pkg-config --cflags glibmm-2.4)

omftool: omftool.o
	$(CXX) -o $@ omftool.o -L../../build/default/libs/pbd -lpbd $(shell pkg-config --libs sqlite3) $(shell pkg-config --libs libxml-2.0)