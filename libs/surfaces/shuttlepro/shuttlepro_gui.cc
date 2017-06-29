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
#include <gtkmm/image.h>

#include "pbd/unwind.h"
#include "pbd/file_utils.h"

#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "pbd/i18n.h"

#include "shuttlepro.h"
#include "jump_distance_widget.h"
#include "button_config_widget.h"

using namespace ArdourSurface;

class ShuttleproGUI : public Gtk::HBox, public PBD::ScopedConnectionList
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

	JumpDistanceWidget _jog_distance;
	void update_jog_distance ();

	void update_action(unsigned int index, ButtonConfigWidget* sender);
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
	, _jog_distance (scp._jog_distance)
{
	std::string data_file_path;
	string name = "shuttlepro.png";
	Searchpath spath(ARDOUR::ardour_data_search_path());
	spath.add_subdirectory_to_paths ("icons");
	find_file (spath, name, data_file_path);
	if (!data_file_path.empty()) {
		Image* image = manage (new Image (data_file_path));
		pack_start (*image, false, false);
	}

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
	table->attach (*jog_label, 0, 2, n, n+1);

	_jog_distance.Changed.connect (*this, invalidator (*this), boost::bind (&ShuttleproGUI::update_jog_distance, this), gui_context ());
	table->attach (_jog_distance, 3, 5, n, n+1);
	++n;

	vector<boost::shared_ptr<ButtonBase> >::const_iterator it;
	unsigned int btn_idx = 0;
	for (it = _scp._button_actions.begin(); it != _scp._button_actions.end(); ++it) {
		Label* lb = manage (new Label (string_compose (_("Setting for button %1"), btn_idx+1)));
		table->attach (*lb, 0, 2, n, n+1);

		ButtonConfigWidget* bcw = manage (new ButtonConfigWidget);

		bcw->set_current_config (*it);
		bcw->Changed.connect (*this, invalidator (*this), boost::bind (&ShuttleproGUI::update_action, this, btn_idx, bcw), gui_context ());
		table->attach (*bcw, 3, 5, n, n+1);
		++n;
		++btn_idx;
	}

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
	_scp._jog_distance = _jog_distance.get_distance ();
}

void
ShuttleproGUI::update_action (unsigned int index, ButtonConfigWidget* sender)
{
	if (index >= _scp._button_actions.size()) {
		DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("ShuttleproGUI::update_action() index out of bounds %1 / %2\n", index, _scp._button_actions.size()));
		return;
	}
	_scp._button_actions[index] = sender->get_current_config (_scp);
	DEBUG_TRACE (DEBUG::ShuttleproControl, string_compose ("update_action () %1\n", index));
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
