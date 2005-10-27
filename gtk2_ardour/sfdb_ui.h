/*
    Copyright (C) 2005 Paul Davis 
    Written by Taybin Rutkin

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


*/

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
