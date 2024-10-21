ICONFLAGS?=-g

AD = ../..
BL = ../../build/libs
TK = ../../build/libs/tk

CXXFLAGS = -Wall -fsanitize=address

CPPFLAGS = -I $(AD)/libs/gtkmm2ext -I $(AD)/libs/widgets -I $(AD)/libs/pbd -I $(AD)/build/libs/pbd
CPPFLAGS += -I $(AD)/libs/tk/ydkmm/ydkmm -I $(AD)/libs/tk/ydk/ydk/
CPPFLAGS += -I $(AD)/libs/tk/ytkmm/ytkmm -I $(AD)/libs/tk/ytk/ytk/
CPPFLAGS += -I $(AD)/libs/tk/ztkmm/ztkmm/ -I $(AD)/libs/tk/ztk/ztk/
CPPFLAGS += -I $(AD)/libs/tk/ydk-pixbuf/ydk-pixbuf/
CPPFLAGS += `pkg-config --cflags cairo pangomm-1.4 cairomm-1.0 libxml-2.0 glibmm-2.4 giomm-2.4`

LDFLAGS = -L$(BL)/pbd -Wl,-rpath=$(BL)/pbd
LDFLAGS += -L$(BL)/gtkmm2ext -Wl,-rpath=$(BL)/gtkmm2ext
LDFLAGS += -L$(BL)/widgets -Wl,-rpath=$(BL)/widgets

LDFLAGS += -L$(TK)/ztk -Wl,-rpath=$(TK)/ztk
LDFLAGS += -L$(TK)/ydk -Wl,-rpath=$(TK)/ydk
LDFLAGS += -L$(TK)/ytk -Wl,-rpath=$(TK)/ytk
LDFLAGS += -L$(TK)/ydk-pixbuf -Wl,-rpath=$(TK)/ydk-pixbuf
LDFLAGS += -L$(TK)/ztkmm -Wl,-rpath=$(TK)/ztkmm
LDFLAGS += -L$(TK)/ydkmm -Wl,-rpath=$(TK)/ydkmm
LDFLAGS += -L$(TK)/ytkmm -Wl,-rpath=$(TK)/ytkmm

LDLIBS = `pkg-config --libs cairo cairomm-1.0 libxml-2.0 glibmm-2.4`
LDLIBS += -lztk -lydk -lytk -lydk-pixbuf -lztkmm -lydkmm -lytkmm
LDLIBS += -lpbd -lgtkmm2ext -lwidgets

icon: icon.cc libwidgets
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ icon.cc $(LDLIBS)

libwidgets:
	cd $(AD); ./waf build --targets=libwidgets

show: icon
	rm -f /tmp/ardour_icons-*.png
	ASAN_OPTIONS=halt_on_error=0 LD_LIBRARY_PATH=$(TK)/ytkmm/:$(TK)/ydkmm:$(TK)/ytk:$(TK)/ydk:$(TK)/ztkmm:$(TK)/ydk-pixbuf:$(TK)/ztk ./icon $(ICONFLAGS) -s 22 -o /tmp/ardour_icons-22.png
	ASAN_OPTIONS=halt_on_error=0 LD_LIBRARY_PATH=$(TK)/ytkmm/:$(TK)/ydkmm:$(TK)/ytk:$(TK)/ydk:$(TK)/ztkmm:$(TK)/ydk-pixbuf:$(TK)/ztk ./icon $(ICONFLAGS) -s 23 -o /tmp/ardour_icons-23.png
	ASAN_OPTIONS=halt_on_error=0 LD_LIBRARY_PATH=$(TK)/ytkmm/:$(TK)/ydkmm:$(TK)/ytk:$(TK)/ydk:$(TK)/ztkmm:$(TK)/ydk-pixbuf:$(TK)/ztk ./icon $(ICONFLAGS) -s 44 -o /tmp/ardour_icons-44.png
	ASAN_OPTIONS=halt_on_error=0 LD_LIBRARY_PATH=$(TK)/ytkmm/:$(TK)/ydkmm:$(TK)/ytk:$(TK)/ydk:$(TK)/ztkmm:$(TK)/ydk-pixbuf:$(TK)/ztk ./icon $(ICONFLAGS) -s 45 -o /tmp/ardour_icons-45.png
	convert -gravity center \
		/tmp/ardour_icons-*.png \
		-background none \
		+append /tmp/ardour_icons.png

	display /tmp/ardour_icons.png &

clean:
	rm -f icon

.PHONY: show libwidgets clean
