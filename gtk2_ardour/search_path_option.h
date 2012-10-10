/*
    Copyright (C) 2000-2010 Paul Davis

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

#ifndef __gtk_ardour_search_path_option_h__
#define __gtk_ardour_search_path_option_h__

#include <string>

#include <gtkmm/filechooserbutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>

#include "option_editor.h"

class SearchPathOption : public Option
{
  public:
        SearchPathOption (const std::string& pathname, const std::string& label,
                          sigc::slot<std::string>, sigc::slot<bool, std::string>);
        ~SearchPathOption ();

        void set_state_from_config ();
        void add_to_page (OptionEditorPage*);
        void clear ();

        Gtk::Widget& tip_widget() { return add_chooser; }

  protected:
	sigc::slot<std::string> _get; ///< slot to get the configuration variable's value
	sigc::slot<bool, std::string> _set;  ///< slot to set the configuration variable's value

        struct PathEntry {
            PathEntry (const std::string& path, bool removable=true);

            Gtk::Entry entry;
            Gtk::Button remove_button;
            Gtk::HBox box;

            std::string path;
        };

        std::list<PathEntry*> paths;
        Gtk::FileChooserButton add_chooser;
        Gtk::VBox vbox;
        Gtk::VBox path_box;
        Gtk::Label session_label;

        void add_path (const std::string& path, bool removable=true);
        void remove_path (PathEntry*);
        void changed ();
        void path_chosen ();
};

#endif /* __gtk_ardour_search_path_option_h__ */
