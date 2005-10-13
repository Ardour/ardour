#ifndef __ardour_sfdb_ui_h__
#define __ardour_sfdb_ui_h__

#include <string>
#include <vector>

#include <sigc++/signal.h>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dialog.h>
#include <gtkmm/filechooserwidget.h>

class SoundFileBrowser : public Gtk::Dialog
{
  public:
    SoundFileBrowser (std::string title);
    virtual ~SoundFileBrowser () {}

  protected:
    Gtk::FileChooserWidget chooser;
};

class SoundFileChooser : public SoundFileBrowser
{
  public:
    SoundFileChooser (std::string title);
    virtual ~SoundFileChooser () {};

    std::string get_filename () {return chooser.get_filename();};
};

class SoundFileOmega : public SoundFileBrowser
{
  public:
    SoundFileOmega (std::string title);
    virtual ~SoundFileOmega () {};

    sigc::signal<void, std::vector<std::string>, bool> Embedded;
    sigc::signal<void, std::vector<std::string>, bool> Imported;

  protected:
    Gtk::Button embed_btn;
    Gtk::Button import_btn;
    Gtk::CheckButton split_check;

    void embed_clicked ();
    void import_clicked ();
};

#endif // __ardour_sfdb_ui_h__
