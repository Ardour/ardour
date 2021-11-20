AD = ../..
CXXFLAGS = -Wall -pthread -g
CPPFLAGS = -I $(AD)/libs/pbd -I $(AD)/build/libs/pbd
CPPFLAGS += `pkg-config --cflags glibmm-2.4`

LDFLAGS = -L$(AD)/build/libs/pbd -Wl,-rpath=$(AD)/build/libs/pbd
LDLIBS  = `pkg-config --libs glibmm-2.4` -lpbd

signal-test: signal-test.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ signal-test.cc $(LDLIBS)

clean:
	rm -f signal-test

test: signal-test
	while test $$? = 0 ; do echo -n "."; ./signal-test; done

.PHONY: clean test
