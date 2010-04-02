test: xpath
	LD_LIBRARY_PATH=..:../../sigc++2:../../glibmm2 ./xpath
	LD_LIBRARY_PATH=..:../../sigc++2:../../glibmm2 gprof ./xpath > gprof.out

xpath: xpath.cc
	gcc -o $@ -g -pg -I.. `xml2-config --libs --cflags` -L.. -L../../sigc++2 -L../../glibmm2 -lstdc++ -lpbd -lglibmm2 -lsigc++2  $< 
