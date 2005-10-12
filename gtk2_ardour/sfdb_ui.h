#ifndef __ardour_sfdb_ui_h__
#define __ardour_sfdb_ui_h__

#include <string>
#include <vector>

#include <gtkmm/button.h>
#include <gtkmm/dialog.h>
#include <gtkmm/filechooserwidget.h>

class SoundFileBrowser : public Gtk::Dialog
{
  public:
    SoundFileBrowser (std::string title);
    virtual ~SoundFileBrowser ();

  protected:
    Gtk::FileChooserWidget* chooser;

    Gtk::Button* ok_btn;
};

class SoundFileChooser : public SoundFileBrowser
{
  public:
    SoundFileChooser (std::string title);
    virtual ~SoundFileChooser ();

    std::string get_filename ();

  protected:
    Gtk::Button* open_btn;
};

class SoundFileOmega : public SoundFileChooser
{
  public:
    SoundFileOmega (std::string title);
    virtual ~SoundFileOmega ();

    std::vector<std::string> get_filenames();
    bool get_split();

  protected:
    Gtk::Button* insert_btn;
    Gtk::Button* import_btn;
}

#endif // __ardour_sfdb_ui_h__
