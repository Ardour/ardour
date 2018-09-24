/*
    Copyright (C) 2018 Paul Davis

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

#include "pbd/enumwriter.h"

#include "temporal/time.h"

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"

#include "widgets/tooltips.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour_ui.h"
#include "transport_masters_dialog.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourWidgets;

TransportMastersWidget::TransportMastersWidget ()
	: table (4, 13)
{
	pack_start (table, PACK_EXPAND_WIDGET, 12);

	col_title[0].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Use")));
	col_title[1].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Name")));
	col_title[2].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Type")));
	col_title[3].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Format/\nBPM")));
	col_title[4].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Current")));
	col_title[5].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Last")));
	col_title[6].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Timestamp")));
	col_title[7].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Delta")));
	col_title[8].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Collect")));
	col_title[9].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Data Source")));
	col_title[10].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Active\nCommands")));
	col_title[11].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Clock\nSynced")));
	col_title[12].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("29.97/30")));

#if 0
	set_tooltip (col_title[12], _("<b>When enabled</b> the external timecode source is assumed to use 29.97 fps instead of 30000/1001.\n"
	                              "SMPTE 12M-1999 specifies 29.97df as 30000/1001. The spec further mentions that "
	                              "drop-sample timecode has an accumulated error of -86ms over a 24-hour period.\n"
	                              "Drop-sample timecode would compensate exactly for a NTSC color frame rate of 30 * 0.9990 (ie 29.970000). "
	                              "That is not the actual rate. However, some vendors use that rate - despite it being against the specs - "
	                              "because the variant of using exactly 29.97 fps has zero timecode drift.\n"
		             ));

	set_tooltip (col_title[11], string_compose (_("<b>When enabled</b> the external timecode source is assumed to be sample-clock synced to the audio interface\n"
	                                              "being used by %1."), PROGRAM_NAME));
#endif

	table.set_spacings (6);

	TransportMasterManager::instance().CurrentChanged.connect (current_connection, invalidator (*this), boost::bind (&TransportMastersWidget::current_changed, this, _1, _2), gui_context());

	rebuild ();
}

TransportMastersWidget::~TransportMastersWidget ()
{
	for (vector<Row*>::iterator r = rows.begin(); r != rows.end(); ++r) {
		delete *r;
	}
}

void
TransportMastersWidget::current_changed (boost::shared_ptr<TransportMaster> old_master, boost::shared_ptr<TransportMaster> new_master)
{
	for (vector<Row*>::iterator r = rows.begin(); r != rows.end(); ++r) {
		if ((*r)->tm == new_master) {
			(*r)->use_button.set_active (true);
			break; /* there can only be one */
		}
	}
}

void
TransportMastersWidget::rebuild ()
{
	TransportMasterManager::TransportMasters const & masters (TransportMasterManager::instance().transport_masters());

	container_clear (table);

	for (vector<Row*>::iterator r = rows.begin(); r != rows.end(); ++r) {
		delete *r;
	}

	rows.clear ();
	table.resize (masters.size()+1, 13);

	for (size_t col = 0; col < sizeof (col_title) / sizeof (col_title[0]); ++col) {
		table.attach (col_title[col], col, col+1, 0, 1);
	}

	uint32_t n = 1;

	for (TransportMasterManager::TransportMasters::const_iterator m = masters.begin(); m != masters.end(); ++m, ++n) {

		Row* r = new Row;
		rows.push_back (r);

		r->tm = *m;
		r->label.set_text ((*m)->name());
		r->type.set_text (enum_2_string  ((*m)->type()));

		r->use_button.set_group (use_button_group);

		if (TransportMasterManager::instance().current() == r->tm) {
			r->use_button.set_active (true);
		}

		int col = 0;

		table.attach (r->use_button, col, col+1, n, n+1); ++col;
		table.attach (r->type, col, col+1, n, n+1); ++col;
		table.attach (r->label, col, col+1, n, n+1); ++col;
		table.attach (r->format, col, col+1, n, n+1); ++col;
		table.attach (r->current, col, col+1, n, n+1); ++col;
		table.attach (r->last, col, col+1, n, n+1); ++col;
		table.attach (r->timestamp, col, col+1, n, n+1); ++col;
		table.attach (r->delta, col, col+1, n, n+1); ++col;
		table.attach (r->collect_button, col, col+1, n, n+1); ++col;
		table.attach (r->port_combo, col, col+1, n, n+1); ++col;
		table.attach (r->request_options, col, col+1, n, n+1); ++col;

		boost::shared_ptr<TimecodeTransportMaster> ttm (boost::dynamic_pointer_cast<TimecodeTransportMaster> (r->tm));

		if (ttm) {
			table.attach (r->sclock_synced_button, col, col+1, n, n+1); ++col;
			table.attach (r->fr2997_button, col, col+1, n, n+1); ++col;
			r->fr2997_button.signal_toggled().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::fr2997_button_toggled));
		}

		r->port_combo.signal_changed().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::port_choice_changed));
		r->use_button.signal_toggled().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::use_button_toggled));
		r->collect_button.signal_toggled().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::collect_button_toggled));
		r->request_options.signal_button_press_event().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::request_option_press), false);

		if (ttm) {
			r->sclock_synced_button.signal_toggled().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::sync_button_toggled));
		}

		r->tm->PropertyChanged.connect (r->property_change_connection, invalidator (*this), boost::bind (&TransportMastersWidget::Row::prop_change, r, _1), gui_context());

		PropertyChange all_change;
		all_change.add (Properties::locked);
		all_change.add (Properties::collect);
		all_change.add (Properties::connected);

		if (ttm) {
			all_change.add (Properties::fr2997);
			all_change.add (Properties::sclock_synced);
		}

		r->prop_change (all_change);
	}
}

TransportMastersWidget::Row::Row ()
	: request_option_menu (0)
	, ignore_active_change (false)
{
}

void
TransportMastersWidget::Row::prop_change (PropertyChange what_changed)
{
	if (what_changed.contains (Properties::locked)) {
	}

	if (what_changed.contains (Properties::fr2997)) {
		fr2997_button.set_active (boost::dynamic_pointer_cast<TimecodeTransportMaster> (tm)->fr2997());
	}

	if (what_changed.contains (Properties::sclock_synced)) {
		sclock_synced_button.set_active (boost::dynamic_pointer_cast<TimecodeTransportMaster> (tm)->sample_clock_synced());
	}

	if (what_changed.contains (Properties::collect)) {
		collect_button.set_active (tm->collect());
	}

	if (what_changed.contains (Properties::connected)) {
		populate_port_combo ();
	}

	if (what_changed.contains (Properties::name)) {
		label.set_text (tm->name());
	}
}

void
TransportMastersWidget::Row::use_button_toggled ()
{
	if (use_button.get_active()) {
		Config->set_sync_source (tm->type());
	}
}

void
TransportMastersWidget::Row::fr2997_button_toggled ()
{
	boost::dynamic_pointer_cast<TimecodeTransportMaster>(tm)->set_fr2997 (fr2997_button.get_active());
}

void
TransportMastersWidget::Row::collect_button_toggled ()
{
	tm->set_collect (collect_button.get_active());
}

void
TransportMastersWidget::Row::sync_button_toggled ()
{
	tm->set_sample_clock_synced (sclock_synced_button.get_active());
}

bool
TransportMastersWidget::Row::request_option_press (GdkEventButton* ev)
{
	if (ev->button == 1) {
		if (!request_option_menu) {
			build_request_options ();
		}
		request_option_menu->popup (1, ev->time);
		return true;
	}
	return false;
}

void
TransportMastersWidget::Row::build_request_options ()
{
	using namespace Gtk::Menu_Helpers;

	request_option_menu = manage (new Menu);

	MenuList& items (request_option_menu->items());

	items.push_back (CheckMenuElem (_("Accept speed-changing commands (start/stop)")));
	Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back ());
	i->set_active (tm->request_mask() & TR_Speed);
	items.push_back (CheckMenuElem (_("Accept locate commands")));
	i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back ());
	i->set_active (tm->request_mask() & TR_Locate);
}

Glib::RefPtr<Gtk::ListStore>
TransportMastersWidget::Row::build_port_list (vector<string> const & ports)
{
	Glib::RefPtr<Gtk::ListStore> store = ListStore::create (port_columns);
	TreeModel::Row row;

	row = *store->append ();
	row[port_columns.full_name] = string();
	row[port_columns.short_name] = _("Disconnected");

	for (vector<string>::const_iterator p = ports.begin(); p != ports.end(); ++p) {

		if (AudioEngine::instance()->port_is_mine (*p)) {
			continue;
		}

		row = *store->append ();
		row[port_columns.full_name] = *p;

		std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*p);
		if (pn.empty ()) {
			pn = (*p).substr ((*p).find (':') + 1);
		}
		row[port_columns.short_name] = pn;
	}

	return store;
}

void
TransportMastersWidget::Row::populate_port_combo ()
{
	if (!tm->port()) {
		port_combo.hide ();
		return;
	} else {
		port_combo.show ();
	}

	vector<string> inputs;

	if (tm->port()->type() == DataType::MIDI) {
		ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput), inputs);
	} else {
		ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::AUDIO, ARDOUR::PortFlags (ARDOUR::IsOutput), inputs);
	}

	Glib::RefPtr<Gtk::ListStore> input = build_port_list (inputs);
	bool input_found = false;
	int n;

	port_combo.set_model (input);

	Gtk::TreeModel::Children children = input->children();
	Gtk::TreeModel::Children::iterator i;
	i = children.begin();
	++i; /* skip "Disconnected" */


	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[port_columns.full_name];
		if (tm->port()->connected_to (port_name)) {
			port_combo.set_active (n);
			input_found = true;
			break;
		}
	}

	if (!input_found) {
		port_combo.set_active (0); /* disconnected */
	}
}

void
TransportMastersWidget::Row::port_choice_changed ()
{
	if (ignore_active_change) {
		return;
	}

	TreeModel::iterator active = port_combo.get_active ();
	string new_port = (*active)[port_columns.full_name];

	if (new_port.empty()) {
		tm->port()->disconnect_all ();
		return;
	}

	if (!tm->port()->connected_to (new_port)) {
		tm->port()->disconnect_all ();
		tm->port()->connect (new_port);
	}
}

void
TransportMastersWidget::Row::update (Session* s, samplepos_t now)
{
	using namespace Timecode;

	samplepos_t pos;
	double speed;
	stringstream ss;
	Time t;
	boost::shared_ptr<TimecodeTransportMaster> ttm;
	boost::shared_ptr<MIDIClock_TransportMaster> mtm;

	if (s) {

		if (tm->speed_and_position (speed, pos, now)) {

			sample_to_timecode (pos, t, false, false, 25, false, AudioEngine::instance()->sample_rate(), 100, false, 0);

			if ((ttm = boost::dynamic_pointer_cast<TimecodeTransportMaster> (tm))) {
				format.set_text (timecode_format_name (ttm->apparent_timecode_format()));
			} else if ((mtm = boost::dynamic_pointer_cast<MIDIClock_TransportMaster> (tm))) {
				char buf[8];
				snprintf (buf, sizeof (buf), "%.1f", mtm->bpm());
				format.set_text (buf);
			} else {
				format.set_text ("");
			}
			current.set_text (Timecode::timecode_format_time (t));
			timestamp.set_markup (tm->position_string());
			delta.set_markup (tm->delta_string ());

		}
	}
}

void
TransportMastersWidget::update (samplepos_t audible)
{
	samplepos_t now = AudioEngine::instance()->sample_time ();

	for (vector<Row*>::iterator r = rows.begin(); r != rows.end(); ++r) {
		(*r)->update (_session, now);
	}
}

void
TransportMastersWidget::on_map ()
{
	update_connection = ARDOUR_UI::Clock.connect (sigc::mem_fun (*this, &TransportMastersWidget::update));
	Gtk::VBox::on_map ();
}

void
TransportMastersWidget::on_unmap ()
{
	update_connection.disconnect ();
	Gtk::VBox::on_unmap ();
}

TransportMastersWindow::TransportMastersWindow ()
	: ArdourWindow (_("Transport Masters"))
{
	add (w);
	w.show ();
}

void
TransportMastersWindow::on_realize ()
{
	ArdourWindow::on_realize ();
	/* (try to) ensure that resizing is possible and the window can be moved (and closed) */
	get_window()->set_decorations (Gdk::DECOR_BORDER | Gdk::DECOR_RESIZEH | Gdk::DECOR_TITLE | Gdk::DECOR_MENU);
}



void
TransportMastersWindow::set_session (ARDOUR::Session* s)
{
	ArdourWindow::set_session (s);
	w.set_session (s);
}
