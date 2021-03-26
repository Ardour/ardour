ICONFLAGS?=-g

AD = ../..
CXXFLAGS = -Wall
CPPFLAGS = -I $(AD)/libs/gtkmm2ext -I $(AD)/libs/widgets -I $(AD)/libs/pbd -I $(AD)/build/libs/pbd
CPPFLAGS += `pkg-config --cflags cairo cairomm-1.0 libxml-2.0 glibmm-2.4 gtkmm-2.4`

LDFLAGS = -L$(AD)/build/libs/pbd -Wl,-rpath=$(AD)/build/libs/pbd
LDFLAGS += -L$(AD)/build/libs/gtkmm2ext -Wl,-rpath=$(AD)/build/libs/gtkmm2ext
LDFLAGS += -L$(AD)/build/libs/widgets -Wl,-rpath=$(AD)/build/libs/widgets

LDLIBS = `pkg-config --libs cairo cairomm-1.0 libxml-2.0 glibmm-2.4` -lpbd -lgtkmm2ext -lwidgets

icon: icon.cc libwidgets
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ icon.cc $(LDLIBS)

libwidgets:
	cd $(AD); ./waf build --targets=libwidgets

show: icon
	rm -f /tmp/ardour_icons-*.png
	./icon $(ICONFLAGS) -s 22 -o /tmp/ardour_icons-22.png
	./icon $(ICONFLAGS) -s 23 -o /tmp/ardour_icons-23.png
	./icon $(ICONFLAGS) -s 44 -o /tmp/ardour_icons-44.png
	./icon $(ICONFLAGS) -s 45 -o /tmp/ardour_icons-45.png
	convert -gravity center \
		/tmp/ardour_icons-*.png \
		-background none \
		+append /tmp/ardour_icons.png

	display /tmp/ardour_icons.png &

clean:
	rm -f icon

.PHONY: show libwidgets clean
