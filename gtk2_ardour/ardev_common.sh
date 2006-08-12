cd $dir/..

#export G_DEBUG=fatal_criticals

export ARDOUR_PATH=gtk2_ardour/glade:gtk2_ardour/pixmaps:gtk2_ardour

export LD_LIBRARY_PATH=libs/surfaces/control_protocol:libs/ardour:libs/midi++2:libs/pbd:libs/soundtouch:libs/gtkmm2ext:libs/sigc++2:libs/glibmm2:libs/gtkmm2/atk:libs/gtkmm2/pango:libs/gtkmm2/gdk:libs/gtkmm2/gtk:libs/libgnomecanvasmm:libs/libsndfile:libs/appleutility:$LD_LIBRARY_PATH

# DYLD_LIBRARY_PATH is for darwin.
export DYLD_LIBRARY_PATH=$LD_LIBRARY_PATH

# LADSPA_PATH for OSX
export LADSPA_PATH=$LADSPA_PATH:/Library/Audio/Plug-Ins/LADSPA
