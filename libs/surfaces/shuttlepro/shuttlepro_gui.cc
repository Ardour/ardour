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

	std::vector<boost::shared_ptr<Gtk::Adjustment> > _shuttle_speed_adjustments;
	void set_shuttle_speed (int index);

	Gtk::Adjustment _jog_distance;
	Gtk::ComboBoxText _jog_unit;
	void update_jog_distance ();
	void update_jog_unit ();
};


using namespace PBD;
using namespace ARDOUR;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;

ShuttleproGUI::ShuttleproGUI (ShuttleproControlProtocol& scp)
	: _scp (scp)
	, _keep_rolling (_("Keep rolling after jumps"))
	, _jog_distance (scp._jog_distance, 0, 100, 0.25)
{
	Table* table = manage (new Table);
	table->set_row_spacings (6);
	table->set_row_spacings (6);
	table->show ();

	int n = 0;

	_keep_rolling.signal_toggled().connect (boost::bind (&ShuttleproGUI::toggle_keep_rolling, this));
	_keep_rolling.set_active (_scp._keep_rolling);
	table->attach (_keep_rolling, 0, 2, n, n+1);
	++n;

	Label* speed_label = manage (new Label (_("Transport speeds for the shuttle positions:")));
	table->attach (*speed_label, 0, 2, n, n+1);

	HBox* speed_box = manage (new HBox);
	for (int i=0; i != ShuttleproControlProtocol::num_shuttle_speeds; ++i) {
		double speed = scp._shuttle_speeds[i];
		boost::shared_ptr<Gtk::Adjustment> adj (new Gtk::Adjustment (speed, 0.0, 100.0, 0.25));
		_shuttle_speed_adjustments.push_back (adj);
		SpinButton* sb = manage (new SpinButton (*adj, 0.25, 2));
		speed_box->pack_start (*sb);
		sb->signal_value_changed().connect (boost::bind (&ShuttleproGUI::set_shuttle_speed, this, i));
	}
	table->attach (*speed_box, 3, 5, n, n+1);
	++n;

	Label* jog_label = manage (new Label (_("Jump distance for jog wheel:")));
	table->attach(*jog_label, 0, 2, n, n+1);

	HBox* jog_box = manage (new HBox);
	SpinButton* jog_sb = manage (new SpinButton (_jog_distance, 0.25, 2));
	jog_sb->signal_value_changed().connect (boost::bind (&ShuttleproGUI::update_jog_distance, this));
	jog_box->pack_start (*jog_sb);

	const vector<string> jog_units_strings ({ _("seconds"), _("beats"), _("bars") });
	set_popdown_strings (_jog_unit, jog_units_strings);
	_jog_unit.set_active(_scp._jog_unit);
	_jog_unit.signal_changed().connect (boost::bind (&ShuttleproGUI::update_jog_unit, this));
	jog_box->pack_start (_jog_unit);
	table->attach(*jog_box, 3, 5, n, n+1);
	++n;

	pack_end (*table, false, false);
}

void
ShuttleproGUI::toggle_keep_rolling ()
{
	_scp._keep_rolling = _keep_rolling.get_active();
}

void
ShuttleproGUI::set_shuttle_speed (int index)
{
	double speed = _shuttle_speed_adjustments[index]->get_value ();
	_scp._shuttle_speeds[index] = speed;
}

void
ShuttleproGUI::update_jog_distance ()
{
	_scp._jog_distance = _jog_distance.get_value ();
}

void
ShuttleproGUI::update_jog_unit ()
{
	_scp._jog_unit = ShuttleproControlProtocol::JogUnit (_jog_unit.get_active_row_number ());
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
