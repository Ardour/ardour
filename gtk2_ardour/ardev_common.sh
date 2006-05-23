#export G_DEBUG=fatal_criticals

export ARDOUR_PATH=./glade:./pixmaps:.:..

export LD_LIBRARY_PATH=../libs/surfaces/control_protocol:../libs/ardour:../libs/midi++2:../libs/pbd3:../libs/soundtouch:../libs/gtkmm2ext:../libs/sigc++2:../libs/glibmm2:../libs/gtkmm2/atk:../libs/gtkmm2/pango:../libs/gtkmm2/gdk:../libs/gtkmm2/gtk:../libs/libgnomecanvasmm:../libs/libglademm:$LD_LIBRARY_PATH

# DYLD_LIBRARY_PATH is for darwin.
export DYLD_LIBRARY_PATH=$LD_LIBRARY_PATH
