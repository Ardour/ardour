/*
    Copyright (C) 2010 Paul Davis

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

#include "pbd/strsplit.h"
#include "pbd/compose.h"
#include "search_path_option.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;

SearchPathOption::SearchPathOption (const string& pathname, const string& label,
                                    sigc::slot<std::string> get, sigc::slot<bool, std::string> set)
        : Option (pathname, label)
        , _get (get)
        , _set (set)
        , add_chooser (_("Select folder to search for media"), FILE_CHOOSER_ACTION_SELECT_FOLDER)
{
        add_chooser.signal_file_set().connect (sigc::mem_fun (*this, &SearchPathOption::path_chosen));

        HBox* hbox = manage (new HBox);

        hbox->set_border_width (12);
        hbox->set_spacing (6);
        hbox->pack_end (add_chooser, true, true);
        hbox->pack_end (*manage (new Label (_("Click to add a new location"))), false, false);
        hbox->show_all ();

        vbox.pack_start (path_box);
        vbox.pack_end (*hbox);

        session_label.set_use_markup (true);
        session_label.set_markup (string_compose ("<i>%1</i>", _("the session folder")));
        session_label.set_alignment (0.0, 0.5);
        session_label.show ();

        path_box.pack_start (session_label);
}

SearchPathOption::~SearchPathOption()
{


}

void
SearchPathOption::path_chosen ()
{
        string path = add_chooser.get_filename ();
        add_path (path);
        changed ();
}

void
SearchPathOption::add_to_page (OptionEditorPage* p)
{
	int const n = p->table.property_n_rows();
	p->table.resize (n + 1, 3);

        Label* label = manage (new Label);
        label->set_alignment (0.0, 0.0);
        label->set_text (string_compose ("%1", _name));

	p->table.attach (*label, 1, 2, n, n + 1, FILL | EXPAND);
	p->table.attach (vbox, 2, 3, n, n + 1, FILL | EXPAND);
}

void
SearchPathOption::clear ()
{
        path_box.remove (session_label);
        for (list<PathEntry*>::iterator p = paths.begin(); p != paths.end(); ++p) {
                path_box.remove ((*p)->box);
                delete *p;
        }
        paths.clear ();
}

void
SearchPathOption::set_state_from_config ()
{
        string str = _get ();
        vector<string> dirs;

        clear ();
        path_box.pack_start (session_label);

        split (str, dirs, ':');

        for (vector<string>::iterator d = dirs.begin(); d != dirs.end(); ++d) {
                add_path (*d);
        }
}

void
SearchPathOption::changed ()
{
        string str;

        for (list<PathEntry*>::iterator p = paths.begin(); p != paths.end(); ++p) {

                if (!str.empty()) {
                        str += ':';
                }
                str += (*p)->entry.get_text ();
        }

        _set (str);
}

void
SearchPathOption::add_path (const string& path, bool removable)
{
        PathEntry* pe = new PathEntry (path, removable);
        paths.push_back (pe);
        path_box.pack_start (pe->box, false, false);
        pe->remove_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (*this, &SearchPathOption::remove_path), pe));
}

void
SearchPathOption::remove_path (PathEntry* pe)
{
        path_box.remove (pe->box);
        paths.remove (pe);
        delete pe;
        changed ();
}

SearchPathOption::PathEntry::PathEntry (const std::string& path, bool removable)
        : remove_button (Stock::REMOVE)
{
        entry.set_text (path);
        entry.show ();

        box.set_spacing (6);
        box.set_homogeneous (false);
        box.pack_start (entry, true, true);

        if (removable) {
                box.pack_start (remove_button, false, false);
                remove_button.show ();
        }

        box.show ();
}
