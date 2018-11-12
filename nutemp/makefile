
AD = ..
CXXFLAGS = -g
CPPFLAGS = -I $(AD)/libs/ardour -I $(AD)/libs/evoral -I $(AD)/libs/pbd -I $(AD)/build/libs/pbd -I $(AD)/libs/temporal
CPPFLAGS += `pkg-config --cflags glibmm-2.4` `pkg-config --cflags libxml-2.0`
LDFLAGS = -L $(AD)/build/libs/evoral -levoral -Xlinker -rpath=$(AD)/build/libs/evoral
LDFLAGS += -L $(AD)/build/libs/pbd -lpbd -Xlinker -rpath=$(AD)/build/libs/pbd
LDFLAGS += -L $(AD)/build/libs/temporal -ltemporal -Xlinker -rpath=$(AD)/build/libs/temporal
LDFLAGS += `pkg-config --cflags --libs glibmm-2.4` `pkg-config --cflags --libs libxml-2.0`

all: t

t.o: t.cc t.h

t: t.o
	$(CXX) $(LDFLAGS) -o $@ $?

clean:
	rm -f t t.o
