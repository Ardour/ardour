AD = ../..
CXXFLAGS = -Wall
CPPFLAGS =  -I $(AD)/libs/evoral -I $(AD)/libs/pbd -I $(AD)/build/libs/pbd
CPPFLAGS += `pkg-config --cflags libxml-2.0 glibmm-2.4`

LDFLAGS = -L $(AD)/build/libs/pbd
LDFLAGS += -L $(AD)/build/libs/temporal
LDFLAGS += -L $(AD)/build/libs/evoral

LDLIBS = -lpbd -Xlinker -rpath=$(AD)/build/libs/pbd
LDLIBS += -ltemporal -Xlinker -rpath=$(AD)/build/libs/temporal
LDLIBS += -levoral -Xlinker -rpath=$(AD)/build/libs/evoral
LDLIBS += `pkg-config --libs libxml-2.0 glibmm-2.4`

smfload: smfload.cc libevoral
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ smfload.cc $(LDLIBS)

libevoral:
	cd $(AD); ./waf build --targets=libevoral

clean:
	rm -f smfload

.PHONY: libevoral clean
