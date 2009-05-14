 /*
    Copyright (C) 2001-2009 Paul Davis

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

#include <gtkmm/box.h>
#include <gtkmm/alignment.h>
#include "ardour/configuration.h"
#include "option_editor.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace Gtk;
using namespace ARDOUR;

void
OptionEditorComponent::add_widget_to_page (OptionEditorPage* p, Gtk::Widget* w)
{
	int const n = p->table.property_n_rows();
	p->table.resize (n + 1, 3);
	p->table.attach (*w, 1, 3, n, n + 1, FILL | EXPAND);
}

void
OptionEditorComponent::add_widgets_to_page (OptionEditorPage* p, Gtk::Widget* wa, Gtk::Widget* wb)
{
	int const n = p->table.property_n_rows();
	p->table.resize (n + 1, 3);
	p->table.attach (*wa, 1, 2, n, n + 1, FILL | EXPAND);
	p->table.attach (*wb, 2, 3, n, n + 1, FILL | EXPAND);
}

OptionEditorHeading::OptionEditorHeading (string const & h)
{
	std::stringstream s;
	s << "<b>" << h << "</b>";
	_label = manage (new Label (s.str()));
	_label->set_alignment (0, 0.5);
	_label->set_use_markup (true);
}

void
OptionEditorHeading::add_to_page (OptionEditorPage* p)
{
	int const n = p->table.property_n_rows();
	p->table.resize (n + 2, 3);

	p->table.attach (*manage (new Label ("")), 0, 3, n, n + 1, FILL | EXPAND);
	p->table.attach (*_label, 0, 3, n + 1, n + 2, FILL | EXPAND);
}

void
OptionEditorBox::add_to_page (OptionEditorPage* p)
{
	add_widget_to_page (p, _box);
}

BoolOption::BoolOption (string const & i, string const & n, slot<bool> g, slot<bool, bool> s)
	: Option (i, n),
	  _get (g),
	  _set (s)
{
	_button = manage (new CheckButton (n));
	_button->set_active (_get ());
	_button->signal_toggled().connect (mem_fun (*this, &BoolOption::toggled));
}

void
BoolOption::add_to_page (OptionEditorPage* p)
{
	add_widget_to_page (p, _button);
}

void
BoolOption::set_state_from_config ()
{
	_button->set_active (_get ());
}

void
BoolOption::toggled ()
{
	_set (_button->get_active ());
}

OptionEditorPage::OptionEditorPage (Gtk::Notebook& n, std::string const & t)
	: table (1, 3)
{
	table.set_spacings (4);
	table.set_col_spacing (0, 32);
	box.pack_start (table, false, false);
	box.set_border_width (4);
	n.append_page (box, t);
}

/** Construct an OptionEditor.
 *  @param o Configuration to edit.
 *  @param t Title for the dialog.
 */
OptionEditor::OptionEditor (Configuration* c, std::string const & t)
	: ArdourDialog (t, false), _config (c)
{
	using namespace Notebook_Helpers;

	set_default_size (300, 300);
	set_wmclass (X_("ardour_preferences"), "Ardour");

	set_name ("Preferences");
	add_events (Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);

	set_border_width (4);

	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (_notebook);

	_notebook.set_show_tabs (true);
	_notebook.set_show_border (true);
	_notebook.set_name ("OptionsNotebook");

	show_all_children();

	/* Watch out for changes to parameters */
	_config->ParameterChanged.connect (mem_fun (*this, &OptionEditor::parameter_changed));
}

OptionEditor::~OptionEditor ()
{
	for (std::map<std::string, OptionEditorPage*>::iterator i = _pages.begin(); i != _pages.end(); ++i) {
		for (std::list<OptionEditorComponent*>::iterator j = i->second->components.begin(); j != i->second->components.end(); ++j) {
			delete *j;
		}
		delete i->second;
	}
}

/** Called when a configuration parameter has been changed.
 *  @param p Parameter name.
 */
void
OptionEditor::parameter_changed (std::string const & p)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &OptionEditor::parameter_changed), p));

	for (std::map<std::string, OptionEditorPage*>::iterator i = _pages.begin(); i != _pages.end(); ++i) {
		for (std::list<OptionEditorComponent*>::iterator j = i->second->components.begin(); j != i->second->components.end(); ++j) {
			(*j)->parameter_changed (p);
		}
	}
}

/** Add a component to a given page.
 *  @param pn Page name (will be created if it doesn't already exist)
 *  @param o Component.
 */
void
OptionEditor::add (std::string const & pn, OptionEditorComponent* o)
{
	if (_pages.find (pn) == _pages.end()) {
		_pages[pn] = new OptionEditorPage (_notebook, pn);
	}

	OptionEditorPage* p = _pages[pn];
	p->components.push_back (o);

	o->add_to_page (p);
	o->set_state_from_config ();
}
