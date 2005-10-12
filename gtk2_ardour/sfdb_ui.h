#ifndef __ardour_sfdb_ui_h__
#define __ardour_sfdb_ui_h__

#include <string>

#include <gtkmm/filechooserdialog.h>

#include <ardour/audio_library.h>

class SoundFileChooser : public Gtk::FileChooserDialog
{
  public:
    SoundFileChooser (std::string title, bool split_makes_sense);
    virtual ~SoundFileChooser ();
};

#endif // __ardour_sfdb_ui_h__
