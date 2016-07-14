/*
    Copyright (C) 2016 Robin Gareus <robin@gareus.org
    Copyright (C) 2009-2012 Paul Davis

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

#include <iostream>
#include <list>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/comboboxtext.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "osc.h"

#include "pbd/i18n.h"

namespace ArdourSurface {

class OSC_GUI : public Gtk::VBox
{
	public:
		OSC_GUI (OSC&);
		~OSC_GUI ();

private:
	Gtk::ComboBoxText debug_combo;
	void debug_changed ();

	OSC& cp;
};


void*
OSC::get_gui () const
{
	if (!gui) {
		const_cast<OSC*>(this)->build_gui ();
	}
	static_cast<Gtk::VBox*>(gui)->show_all();
	return gui;
}

void
OSC::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete (OSC_GUI*) gui;
	gui = 0;
}

void
OSC::build_gui ()
{
	gui = (void*) new OSC_GUI (*this);
}

} // end namespace

///////////////////////////////////////////////////////////////////////////////

using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ArdourSurface;

OSC_GUI::OSC_GUI (OSC& p)
	: cp (p)
{
	int n = 0; // table row
	Table* table = manage (new Table);
	Label* label;

	label = manage (new Gtk::Label(_("Connection:")));
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	label = manage (new Gtk::Label(cp.get_server_url()));
	table->attach (*label, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	++n;

	label = manage (new Gtk::Label(_("Debug:")));
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (debug_combo, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);

	std::vector<std::string> debug_options;
	debug_options.push_back (_("Off"));
	debug_options.push_back (_("Log invalid messages"));
	debug_options.push_back (_("Log all messages"));

	set_popdown_strings (debug_combo, debug_options);
	debug_combo.set_active ((int)cp.get_debug_mode());

	table->show_all ();
	pack_start (*table, false, false);

	debug_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::debug_changed));
}

OSC_GUI::~OSC_GUI ()
{
}

void
OSC_GUI::debug_changed ()
{
	std::string str = debug_combo.get_active_text ();
	if (str == _("Off")) {
		cp.set_debug_mode (OSC::Off);
	}
	else if (str == _("Log invalid messages")) {
		cp.set_debug_mode (OSC::Unhandled);
	}
	else if (str == _("Log all messages")) {
		cp.set_debug_mode (OSC::All);
	}
	else {
		std::cerr << "Invalid OSC Debug Mode\n";
		assert (0);
	}
}
