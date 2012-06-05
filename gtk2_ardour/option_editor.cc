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
#include "gtkmm2ext/utils.h"

#include "ardour/configuration.h"
#include "ardour/rc_configuration.h"
#include "ardour/utils.h"
#include "ardour/dB.h"

#include "option_editor.h"
#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;

void
OptionEditorComponent::add_widget_to_page (OptionEditorPage* p, Gtk::Widget* w)
{
	int const n = p->table.property_n_rows();
	int m = n + 1;
	if (!_note.empty ()) {
		++m;
	}

	p->table.resize (m, 3);
	p->table.attach (*w, 1, 3, n, n + 1, FILL | EXPAND);

	maybe_add_note (p, n + 1);
}

void
OptionEditorComponent::add_widgets_to_page (OptionEditorPage* p, Gtk::Widget* wa, Gtk::Widget* wb)
{
	int const n = p->table.property_n_rows();
	int m = n + 1;
	if (!_note.empty ()) {
		++m;
	}
	
	p->table.resize (m, 3);
	p->table.attach (*wa, 1, 2, n, n + 1, FILL);
	p->table.attach (*wb, 2, 3, n, n + 1, FILL | EXPAND);
	
	maybe_add_note (p, n + 1);
}

void
OptionEditorComponent::maybe_add_note (OptionEditorPage* p, int n)
{
	if (!_note.empty ()) {
		Gtk::Label* l = manage (new Gtk::Label (string_compose (X_("<i>%1</i>"), _note)));
		l->set_use_markup (true);
		p->table.attach (*l, 1, 3, n, n + 1, FILL | EXPAND);
	}
}

void
OptionEditorComponent::set_note (string const & n)
{
	_note = n;
}

OptionEditorHeading::OptionEditorHeading (string const & h)
{
	std::stringstream s;
	s << "<b>" << h << "</b>";
	_label = manage (left_aligned_label (s.str()));
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

BoolOption::BoolOption (string const & i, string const & n, sigc::slot<bool> g, sigc::slot<bool, bool> s)
	: Option (i, n),
	  _get (g),
	  _set (s)
{
	_button = manage (new CheckButton (n));
	_button->set_active (_get ());
	_button->signal_toggled().connect (sigc::mem_fun (*this, &BoolOption::toggled));
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

EntryOption::EntryOption (string const & i, string const & n, sigc::slot<string> g, sigc::slot<bool, string> s)
	: Option (i, n),
	  _get (g),
	  _set (s)
{
	_label = manage (left_aligned_label (n + ":"));
	_entry = manage (new Entry);
	_entry->signal_activate().connect (sigc::mem_fun (*this, &EntryOption::activated));
}

void
EntryOption::add_to_page (OptionEditorPage* p)
{
	add_widgets_to_page (p, _label, _entry);
}

void
EntryOption::set_state_from_config ()
{
	_entry->set_text (_get ());
}

void
EntryOption::activated ()
{
	_set (_entry->get_text ());
}

FaderOption::FaderOption (string const & i, string const & n, sigc::slot<gain_t> g, sigc::slot<bool, gain_t> s)
	: Option (i, n)
	, _db_adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain()), 0, 1, 0.01, 0.1)
	, _get (g)
	, _set (s)
{
	_pix = ::get_icon (X_("fader_belt_h"));
	if (_pix == 0) {
		throw failed_constructor ();
	}

	_pix_desensitised = ::get_icon (X_("fader_belt_h_desensitised"));
	if (_pix_desensitised == 0) {
		throw failed_constructor ();
	}
	
	_db_slider = manage (new HSliderController (_pix,
						    _pix_desensitised,
						    &_db_adjustment,
						    115,
						    false));

	_label.set_text (n + ":");
	_label.set_name (X_("OptionsLabel"));

	_fader_centering_box.pack_start (*_db_slider, true, false);

	_box.set_spacing (4);
	_box.set_homogeneous (false);
	_box.pack_start (_fader_centering_box, false, false);
	_box.pack_start (_db_display, false, false);
	_box.show_all ();

	set_size_request_to_display_given_text (_db_display, "-99.00", 12, 12);

	_db_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &FaderOption::db_changed));
}

void
FaderOption::set_state_from_config ()
{
	gain_t const val = _get ();
	_db_adjustment.set_value (gain_to_slider_position_with_max (val, Config->get_max_gain ()));

	char buf[16];

	if (val == 0.0) {
		snprintf (buf, sizeof (buf), "-inf");
	} else {
		snprintf (buf, sizeof (buf), "%.2f", accurate_coefficient_to_dB (val));
	}

	_db_display.set_text (buf);
}

void
FaderOption::db_changed ()
{
	_set (slider_position_to_gain_with_max (_db_adjustment.get_value (), Config->get_max_gain()));
}

void
FaderOption::add_to_page (OptionEditorPage* p)
{
	add_widgets_to_page (p, &_label, &_box);
}

ClockOption::ClockOption (string const & i, string const & n, sigc::slot<framecnt_t> g, sigc::slot<bool, framecnt_t> s)
	: Option (i, n)
	, _clock (X_("timecode-offset"), false, X_(""), true, false, true, false)
	, _get (g)
	, _set (s)
{
	_label.set_text (n + ":");
	_label.set_alignment (0, 0.5);
	_label.set_name (X_("OptionsLabel"));
}

void
ClockOption::set_state_from_config ()
{
	_clock.set (_get ());
}

void
ClockOption::add_to_page (OptionEditorPage* p)
{
	add_widgets_to_page (p, &_label, &_clock);
}

void
ClockOption::set_session (Session* s)
{
	_clock.set_session (s);
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
	: ArdourWindow (t), _config (c)
{
	using namespace Notebook_Helpers;

	set_default_size (300, 300);
	set_wmclass (X_("ardour_preferences"), PROGRAM_NAME);

	set_name ("Preferences");
	add_events (Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);

	set_border_width (4);

	add (_notebook);

	_notebook.set_show_tabs (true);
	_notebook.set_show_border (true);
	_notebook.set_name ("OptionsNotebook");

	show_all_children();

	/* Watch out for changes to parameters */
	_config->ParameterChanged.connect (config_connection, invalidator (*this), boost::bind (&OptionEditor::parameter_changed, this, _1), gui_context());
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
	ENSURE_GUI_THREAD (*this, &OptionEditor::parameter_changed, p)

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
OptionEditor::add_option (std::string const & pn, OptionEditorComponent* o)
{
	if (_pages.find (pn) == _pages.end()) {
		_pages[pn] = new OptionEditorPage (_notebook, pn);
	}

	OptionEditorPage* p = _pages[pn];
	p->components.push_back (o);

	o->add_to_page (p);
	o->set_state_from_config ();
}

void
OptionEditor::set_current_page (string const & p)
{
	int i = 0;
	while (i < _notebook.get_n_pages ()) {
		if (_notebook.get_tab_label_text (*_notebook.get_nth_page (i)) == p) {
			_notebook.set_current_page (i);
			return;
		}

		++i;
	}
}


DirectoryOption::DirectoryOption (string const & i, string const & n, sigc::slot<string> g, sigc::slot<bool, string> s)
	: Option (i, n)
	, _get (g)
	, _set (s)
{
	_file_chooser.set_action (Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
	_file_chooser.signal_file_set().connect (sigc::mem_fun (*this, &DirectoryOption::file_set));
	_file_chooser.signal_current_folder_changed().connect (sigc::mem_fun (*this, &DirectoryOption::current_folder_set));
}


void
DirectoryOption::set_state_from_config ()
{
	_file_chooser.set_current_folder (_get ());
}

void
DirectoryOption::add_to_page (OptionEditorPage* p)
{
	add_widgets_to_page (p, manage (new Label (_name)), &_file_chooser);
}

void
DirectoryOption::file_set ()
{
	_set (_file_chooser.get_filename ());
}

void
DirectoryOption::current_folder_set ()
{
	_set (_file_chooser.get_current_folder ());
}
