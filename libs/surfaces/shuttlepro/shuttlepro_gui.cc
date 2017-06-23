/*
    Copyright (C) 2009-2013 Paul Davis
    Author: Johannes Mueller

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


#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/liststore.h>

#include "pbd/unwind.h"

#include "ardour/audioengine.h"
#include "ardour/port.h"
#include "ardour/midi_port.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "pbd/i18n.h"

#include "shuttlepro.h"

using namespace ArdourSurface;

class ShuttleproGUI : public Gtk::VBox
{
public:
	ShuttleproGUI (ShuttleproControlProtocol& scp);
	~ShuttleproGUI () {}

private:
	ShuttleproControlProtocol& _scp;

	Gtk::CheckButton _keep_rolling;
	void toggle_keep_rolling ();
};


using namespace PBD;
using namespace ARDOUR;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

ShuttleproGUI::ShuttleproGUI (ShuttleproControlProtocol& scp)
	: _scp (scp)
	, _keep_rolling (_("Keep rolling after jumps"))
{
	Table* table = manage (new Table);
	table->set_row_spacings (6);
	table->set_row_spacings (6);
	table->show ();

	int n = 0;

	_keep_rolling.signal_toggled().connect (sigc::mem_fun (*this, &ShuttleproGUI::toggle_keep_rolling));
	_keep_rolling.set_active (_scp.get_keep_rolling());
	table->attach (_keep_rolling, 0, 2, n, n+1);
	++n;

	pack_start (*table, false, false);
}

void
ShuttleproGUI::toggle_keep_rolling ()
{
	_scp.set_keep_rolling (_keep_rolling.get_active ());
}


void*
ShuttleproControlProtocol::get_gui () const
{
	if (!_gui) {
		const_cast<ShuttleproControlProtocol*>(this)->build_gui ();
	}
	static_cast<Gtk::VBox*>(_gui)->show_all();
	return _gui;
}

void
ShuttleproControlProtocol::tear_down_gui ()
{
	if (_gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(_gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete (ShuttleproGUI*) _gui;
	_gui = 0;
}

void
ShuttleproControlProtocol::build_gui ()
{
	_gui = (void*) new ShuttleproGUI (*this);
}
