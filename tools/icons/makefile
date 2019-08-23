AD = ../..
CXXFLAGS = -Wall
CPPFLAGS = -I $(AD)/libs/gtkmm2ext -I $(AD)/libs/widgets -I $(AD)/libs/pbd -I $(AD)/build/libs/pbd
CPPFLAGS += `pkg-config --cflags cairo cairomm-1.0 libxml-2.0 glibmm-2.4`

LDFLAGS = -L $(AD)/build/libs/pbd -lpbd -Xlinker -rpath=$(AD)/build/libs/pbd
LDFLAGS += -L $(AD)/build/libs/gtkmm2ext -lgtkmm2ext -Xlinker -rpath=$(AD)/build/libs/gtkmm2ext
LDFLAGS += -L $(AD)/build/libs/widgets -lwidgets -Xlinker -rpath=$(AD)/build/libs/widgets

LOADLIBES = `pkg-config  --libs cairo cairomm-1.0 libxml-2.0 glibmm-2.4`

icon: icon.cc

clean:
	rm -f icon
