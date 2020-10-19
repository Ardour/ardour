/*
 * Copyright (C) 2018-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pbd/enumwriter.h"
#include "pbd/unwind.h"

#include "temporal/time.h"

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"

#include "widgets/tooltips.h"
#include "widgets/ardour_icon.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour_ui.h"
#include "floating_text_entry.h"
#include "transport_masters_dialog.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourWidgets;

TransportMastersWidget::TransportMastersWidget ()
	: table (4, 13)
	, add_master_button (_("Add a new Transport Master"))
	, lost_sync_button (_("Keep rolling if sync is lost"))
	, ignore_active_change (false)
{
	midi_port_store = ListStore::create (port_columns);
	audio_port_store = ListStore::create (port_columns);

	AudioEngine::instance()->PortRegisteredOrUnregistered.connect (port_reg_connection, invalidator (*this),  boost::bind (&TransportMastersWidget::update_ports, this), gui_context());
	AudioEngine::instance()->PortPrettyNameChanged.connect (port_reg_connection, invalidator (*this),  boost::bind (&TransportMastersWidget::update_ports, this), gui_context());
	update_ports ();

	Gtk::Table *add_table = manage(new Gtk::Table(1,2));
	add_table->attach(add_master_button, 0,1, 0,1, Gtk::SHRINK);

	pack_start (table, FALSE, FALSE, 12);
	pack_start (*add_table, FALSE, FALSE);
	pack_start (lost_sync_button, FALSE, FALSE, 12);

	Config->ParameterChanged.connect (config_connection, invalidator (*this), boost::bind (&TransportMastersWidget::param_changed, this, _1), gui_context());
	lost_sync_button.signal_toggled().connect (sigc::mem_fun (*this, &TransportMastersWidget::lost_sync_button_toggled));
	lost_sync_button.set_active (Config->get_transport_masters_just_roll_when_sync_lost());
	set_tooltip (lost_sync_button, string_compose (_("<b>When enabled</b>, if the signal from a transport master is lost, %1 will keep rolling at its current speed.\n"
	                                                 "<b>When disabled</b>, loss of transport master sync causes %1 to stop"), PROGRAM_NAME));

	add_master_button.signal_clicked.connect (sigc::mem_fun (*this, &TransportMastersWidget::add_master));

	col_title[0].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Select")));               align[0]=0.0;
	col_title[1].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Name")));                 align[1]=0.5;
	col_title[2].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Type")));                 align[2]=0.5;
	col_title[3].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Source")));               align[3]=0.5;
	col_title[4].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Format")));               align[4]=0.5;
	col_title[5].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Sync Position + Delta")));align[5]=0.5;
	col_title[6].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Last Message + Age")));   align[6]=0.5;
	col_title[7].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Active\nCommands")));     align[7]=0.5;
	col_title[8].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Clock\nSynced")));        align[9]=0.0;
	col_title[9].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("29.97/\n30")));          align[10]=0.0;
	col_title[10].set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Remove")));              align[11]=0.5;

	set_tooltip (col_title[7], _("Controls whether or not certain transport-related commands can be sent from the GUI or control "
	                              "surfaces when this transport master is in use. The default is not to allow any such commands "
	                              "when the master is in use."));

	set_tooltip (col_title[9], _("<b>When enabled</b> the external timecode source is assumed to use 29.97 fps instead of 30000/1001.\n"
	                              "SMPTE 12M-1999 specifies 29.97df as 30000/1001. The spec further mentions that "
	                              "drop-sample timecode has an accumulated error of -86ms over a 24-hour period.\n"
	                              "Drop-sample timecode would compensate exactly for a NTSC color frame rate of 30 * 0.9990 (ie 29.970000). "
	                              "That is not the actual rate. However, some vendors use that rate - despite it being against the specs - "
	                              "because the variant of using exactly 29.97 fps has zero timecode drift.\n"
		             ));

	set_tooltip (col_title[8], string_compose (_("<b>When enabled</b> the external timecode source is assumed to be sample-clock synced to the audio interface\n"
	                                              "being used by %1."), PROGRAM_NAME));

	table.set_col_spacings (12);
	table.set_row_spacings (6);

	TransportMasterManager::instance().CurrentChanged.connect (current_connection, invalidator (*this), boost::bind (&TransportMastersWidget::current_changed, this, _1, _2), gui_context());
	TransportMasterManager::instance().Added.connect (add_connection, invalidator (*this), boost::bind (&TransportMastersWidget::rebuild, this), gui_context());
	TransportMasterManager::instance().Removed.connect (remove_connection, invalidator (*this), boost::bind (&TransportMastersWidget::rebuild, this), gui_context());

	AudioEngine::instance()->Running.connect (engine_running_connection, invalidator (*this), boost::bind (&TransportMastersWidget::update_usability, this), gui_context());

	rebuild ();
}

TransportMastersWidget::~TransportMastersWidget ()
{
	for (vector<Row*>::iterator r = rows.begin(); r != rows.end(); ++r) {
		delete *r;
	}
}

void
TransportMastersWidget::set_transport_master (boost::shared_ptr<TransportMaster> tm)
{
	_session->request_sync_source (tm);
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
TransportMastersWidget::add_master ()
{
	printf ("TransportMastersWidget::add_master\n");
	AddTransportMasterDialog d;

	d.present ();
	string name;

	while (name.empty()) {

		int r = d.run ();

		switch (r) {
		case RESPONSE_ACCEPT:
			name = d.get_name();
			break;
		default:
			return;
		}
	}

	d.hide ();

	if (TransportMasterManager::instance().add (d.get_type(), name)) {
		MessageDialog msg (_("New transport master not added - check error log for details"));
		msg.run ();
	}
}

void
TransportMastersWidget::clear ()
{
	container_clear (table);

	for (vector<Row*>::iterator r = rows.begin(); r != rows.end(); ++r) {
		delete *r;
	}

	rows.clear ();
}

void
TransportMastersWidget::rebuild ()
{
	TransportMasterManager::TransportMasters const & masters (TransportMasterManager::instance().transport_masters());

	clear ();
	table.resize (masters.size()+1, 14);

	for (size_t col = 0; col < sizeof (col_title) / sizeof (col_title[0]); ++col) {
		table.attach (col_title[col], col, col+1, 0, 1);
		col_title[col].set_alignment( align[col], 0.5);
	}

	uint32_t n = 1;

	Gtk::RadioButtonGroup use_button_group;

	for (TransportMasterManager::TransportMasters::const_iterator m = masters.begin(); m != masters.end(); ++m, ++n) {

		Row* r = new Row (*this);
		rows.push_back (r);

		r->tm = *m;
		r->label.set_text ((*m)->name());
		r->type.set_text (enum_2_string  ((*m)->type()));

		r->use_button.set_group (use_button_group);

		if (TransportMasterManager::instance().current() == r->tm) {
			r->use_button.set_active (true);
		}

		int col = 0;

		r->label_box.add (r->label);
		r->current_box.add (r->current);
		r->last_box.add (r->last);

		table.attach (r->use_button,      col, col+1, n, n+1, FILL, SHRINK); ++col;
		table.attach (r->label_box,       col, col+1, n, n+1, FILL, SHRINK); ++col;
		table.attach (r->type,            col, col+1, n, n+1, FILL, SHRINK); ++col;
		table.attach (r->port_combo,      col, col+1, n, n+1, FILL, SHRINK); ++col;
		table.attach (r->format,          col, col+1, n, n+1, FILL, SHRINK); ++col;
		table.attach (r->current_box,     col, col+1, n, n+1, FILL, SHRINK); ++col;
		table.attach (r->last_box,        col, col+1, n, n+1, FILL, SHRINK); ++col;
		table.attach (r->request_options, col, col+1, n, n+1, FILL, SHRINK); ++col;

		boost::shared_ptr<TimecodeTransportMaster> ttm (boost::dynamic_pointer_cast<TimecodeTransportMaster> (r->tm));

		if (ttm) {
			table.attach (r->sclock_synced_button, col, col+1, n, n+1, FILL, SHRINK); ++col;
			table.attach (r->fr2997_button,        col, col+1, n, n+1, FILL, SHRINK); ++col;
			r->fr2997_button.signal_toggled().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::fr2997_button_toggled));
		} else {
			col += 2;
		}

		if (r->tm->removeable()) {
			table.attach (r->remove_button, col, col+1, n, n+1, SHRINK, EXPAND|FILL);
		}

		table.show_all ();

		r->label_box.signal_button_press_event().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::name_press));
		r->port_combo.signal_changed().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::port_choice_changed));
		r->use_button.signal_toggled().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::use_button_toggled));
		r->request_options.signal_button_press_event().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::request_option_press), false);
		r->remove_button.signal_clicked.connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::remove_clicked));

		if (ttm) {
			r->sclock_synced_button.signal_toggled().connect (sigc::mem_fun (*r, &TransportMastersWidget::Row::sync_button_toggled));
		}

		r->tm->PropertyChanged.connect (r->property_change_connection, invalidator (*this), boost::bind (&TransportMastersWidget::Row::prop_change, r, _1), gui_context());

		PropertyChange all_change;
		all_change.add (Properties::locked);
		all_change.add (Properties::collect);
		all_change.add (Properties::connected);
		all_change.add (Properties::allowed_transport_requests);

		if (ttm) {
			all_change.add (Properties::fr2997);
			all_change.add (Properties::sclock_synced);
		}

		r->prop_change (all_change);
	}

	update_usability ();
}

bool
TransportMastersWidget::idle_remove (TransportMastersWidget::Row* row)
{
	TransportMasterManager::instance().remove (row->tm->name());
	return false;
}

void
TransportMastersWidget::update_ports ()
{
	if (!is_mapped()) {
		return;
	}

	{
		PBD::Unwinder<bool> uw (ignore_active_change, true);
		vector<string> inputs;

		ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput), inputs);
		build_port_model (midi_port_store, inputs);

		inputs.clear ();

		ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::AUDIO, ARDOUR::PortFlags (ARDOUR::IsOutput), inputs);
		build_port_model (audio_port_store, inputs);
	}

	for (vector<Row*>::iterator r = rows.begin(); r != rows.end(); ++r) {
		if ((*r)->tm->port()) {
			(*r)->build_port_list ((*r)->tm->port()->type());
		}
	}
}

void
TransportMastersWidget::update_usability ()
{
	for (vector<Row*>::iterator r= rows.begin(); r != rows.end(); ++r) {
		const bool usable = (*r)->tm->usable();
		(*r)->use_button.set_sensitive (usable);
		(*r)->request_options.set_sensitive (usable);
	}
}

TransportMastersWidget::Row::Row (TransportMastersWidget& p)
	: parent (p)
	, request_option_menu (0)
	, name_editor (0)
	, save_when (0)
	, save_last (" --:--:--:--")
{
	remove_button.set_icon (ArdourIcon::CloseCross);
	format.modify_font (UIConfiguration::instance().get_BigMonospaceFont());
	last.modify_font (UIConfiguration::instance().get_BigMonospaceFont());
	current.modify_font (UIConfiguration::instance().get_BigMonospaceFont());

	uint32_t bg = UIConfigurationBase::instance().color ("clock: background");
	uint32_t fg = UIConfigurationBase::instance().color ("clock: text");
	Gdk::Color bg_color = ARDOUR_UI_UTILS::gdk_color_from_rgba (bg);
	Gdk::Color fg_color = ARDOUR_UI_UTILS::gdk_color_from_rgba (fg);

	current_box.modify_bg (Gtk::STATE_NORMAL, bg_color);
	current.modify_fg (Gtk::STATE_NORMAL, fg_color);

	last_box.modify_bg (Gtk::STATE_NORMAL, bg_color);
	last.modify_fg (Gtk::STATE_NORMAL, fg_color);

	set_size_request_to_display_given_text (format, "999.9 BPM", 0, 0);

}

TransportMastersWidget::Row::~Row ()
{
	delete request_option_menu;
}

bool
TransportMastersWidget::Row::name_press (GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1) {
		Gtk::Window* toplevel = dynamic_cast<Gtk::Window*> (label.get_toplevel());
		if (!toplevel) {
			return false;
		}
		name_editor = new FloatingTextEntry (toplevel, tm->name());
		name_editor->use_text.connect (sigc::mem_fun (*this, &TransportMastersWidget::Row::name_edited));
		name_editor->show ();

		/* Now move the floating text entry window to be perfectly
		 * aligned with the upper left corner of the name/label box.
		 */

		Gtk::Widget* tl = label_box.get_toplevel();
		Gtk::Window* top_level = dynamic_cast<Gtk::Window*>(tl);

		if (top_level) {
			Glib::RefPtr<Gdk::Window> win (top_level->get_window());
			int rx, ry;
			win->get_position (rx, ry);
			Gtk::Allocation alloc = label_box.get_allocation();
			name_editor->move (rx + alloc.get_x(), ry + alloc.get_y());
		}

		return true;
	}

	return false;
}

void
TransportMastersWidget::build_port_model (Glib::RefPtr<Gtk::ListStore> model, vector<string> const & ports)
{
	TreeModel::Row row;

	model->clear ();

	row = *model->append ();
	row[port_columns.full_name] = string();
	row[port_columns.short_name] = _("Disconnected");

	for (vector<string>::const_iterator p = ports.begin(); p != ports.end(); ++p) {

		if (AudioEngine::instance()->port_is_mine (*p)) {
			continue;
		}

		row = *model->append ();
		row[port_columns.full_name] = *p;

		std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*p);
		if (pn.empty ()) {
			pn = (*p).substr ((*p).find (':') + 1);
		}
		row[port_columns.short_name] = pn;
	}
}

void
TransportMastersWidget::Row::remove_clicked ()
{
	/* have to do this via an idle callback, because it will destroy the
	   widget from which this callback was initiated.
	*/
	Glib::signal_idle().connect (sigc::bind (sigc::mem_fun (parent, &TransportMastersWidget::idle_remove), this));
}

void
TransportMastersWidget::Row::name_edited (string str, int ignored)
{
	tm->set_name (str);
	/* floating text entry deletes itself */
	name_editor = 0;
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
	}

	if (what_changed.contains (Properties::connected)) {
		populate_port_combo ();
	}

	if (what_changed.contains (Properties::name)) {
		label.set_text (tm->name());
	}

	if (what_changed.contains (Properties::allowed_transport_requests)) {
		request_options.set_text (tm->allowed_request_string());
	}
}

void
TransportMastersWidget::Row::use_button_toggled ()
{
	if (use_button.get_active()) {
		parent.set_transport_master (tm);
	}
}

void
TransportMastersWidget::Row::fr2997_button_toggled ()
{
	boost::dynamic_pointer_cast<TimecodeTransportMaster>(tm)->set_fr2997 (fr2997_button.get_active());
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

	request_option_menu = new Menu;

	MenuList& items (request_option_menu->items());

	items.push_back (CheckMenuElem (_("Accept start/stop commands")));
	Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back ());
	i->set_active (tm->request_mask() & TR_StartStop);
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &TransportMastersWidget::Row::mod_request_type), TR_StartStop));

	items.push_back (CheckMenuElem (_("Accept speed-changing commands")));
	i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back ());
	i->set_active (tm->request_mask() & TR_Speed);
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &TransportMastersWidget::Row::mod_request_type), TR_Speed));

	items.push_back (CheckMenuElem (_("Accept locate commands")));
	i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back ());
	i->set_active (tm->request_mask() & TR_Locate);
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &TransportMastersWidget::Row::mod_request_type), TR_Locate));
}

void
TransportMastersWidget::Row::mod_request_type (TransportRequestType t)
{
	tm->set_request_mask (TransportRequestType ((tm->request_mask() & t) ? (tm->request_mask() & ~t) : (tm->request_mask() | t)));
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

	build_port_list (tm->port()->type());
}

void
TransportMastersWidget::Row::build_port_list (DataType type)
{
	Glib::RefPtr<Gtk::ListStore> input = (type == DataType::MIDI ? parent.midi_port_store : parent.audio_port_store);
	bool input_found = false;
	int n;

	if (input->children().empty()) {
		return;
	}

	port_combo.set_model (input);

	Gtk::TreeModel::Children children = input->children();
	Gtk::TreeModel::Children::iterator i;
	i = children.begin();
	++i; /* skip "Disconnected" */

	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[parent.port_columns.full_name];
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
	if (!tm->port()) {
		return;
	}

	if (parent.ignore_active_change) {
		return;
	}

	TreeModel::iterator active = port_combo.get_active ();
	string new_port = (*active)[parent.port_columns.full_name];

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
	samplepos_t most_recent;
	samplepos_t when;
	stringstream ss;
	Time t;
	boost::shared_ptr<TimecodeTransportMaster> ttm;
	boost::shared_ptr<MIDIClock_TransportMaster> mtm;

	if (!AudioEngine::instance()->running() || !s) {
		return;
	}

	string current_str (" --:--:--:--");
	string delta_str ("\u0394  ----  ");
	string age_str ("         ");

	if (tm->speed_and_position (speed, pos, most_recent, when, now)) {

		if ((ttm = boost::dynamic_pointer_cast<TimecodeTransportMaster> (tm))) {
			Timecode::TimecodeFormat fmt = ttm->apparent_timecode_format();
			format.set_text (timecode_format_name (fmt));

			sample_to_timecode (pos, t, false, false,
					Timecode::timecode_to_frames_per_second (fmt),
					Timecode::timecode_has_drop_frames (fmt),
					AudioEngine::instance()->sample_rate(), 0, false, 0);

		} else if ((mtm = boost::dynamic_pointer_cast<MIDIClock_TransportMaster> (tm))) {
			char buf[16];
			snprintf (buf, sizeof (buf), "%.1f BPM", mtm->bpm());
			buf[15] = '\0';
			format.set_text (buf);
			s->sample_to_timecode (pos, t, false, false);
		} else {
			format.set_text (" - ");
			s->sample_to_timecode (pos, t, false, false);
		}

		current_str = Timecode::timecode_format_time (t);

		delta_str = tm->delta_string ();
		save_when = when;
		save_last = current_str;
	} else {
		format.set_text ("   ?   ");
	}

	if (save_when) {
		char gap[32];
		float seconds = (now - save_when) / (float) AudioEngine::instance()->sample_rate();
		if (seconds < 0) {
			seconds = 0;
		}
		if (seconds < 1.f) {
			snprintf (gap, sizeof (gap), "%3.02fs ago", seconds);
		} else if (seconds < 60.f) {
			snprintf (gap, sizeof (gap), "%3.0fs ago", seconds);
		} else if (seconds < 3600.f) {
			snprintf (gap, sizeof (gap), "%3.0fm ago", seconds / 60.f);
		} else {
			snprintf (gap, sizeof (gap), "%3.0fh ago", seconds/3600.f);
		}
		gap[31] = '\0';
		age_str = gap;
	}

	last.set_text (string_compose (_("%1 %2"), save_last, age_str));
	current.set_text (string_compose ("%1  %2", current_str, delta_str));
}

void
TransportMastersWidget::update (timepos_t /* audible */)
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
	update_ports ();
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

void
TransportMastersWidget::set_session (ARDOUR::Session* s)
{
	session_config_connection.disconnect ();

	SessionHandlePtr::set_session (s);

	if (_session) {
		_session->config.ParameterChanged.connect (session_config_connection, invalidator (*this), boost::bind (&TransportMastersWidget::param_changed, this, _1), gui_context());
		allow_master_select (!_session->config.get_external_sync());
	}
}

TransportMastersWidget::AddTransportMasterDialog::AddTransportMasterDialog ()
	: ArdourDialog (_("Add Transport Master"), true, false)
	, name_label (_("Name"))
	, type_label (_("Type"))
{
	name_hbox.set_spacing (6);
	name_hbox.pack_start (name_label, false, false);
	name_hbox.pack_start (name_entry, true, true);

	type_hbox.set_spacing (6);
	type_hbox.pack_start (type_label, false, false);
	type_hbox.pack_start (type_combo, true, true);

	vector<string> s;

	s.push_back (X_("MTC"));
	s.push_back (X_("LTC"));
	s.push_back (X_("MIDI Clock"));

	set_popdown_strings (type_combo, s);
	type_combo.set_active_text (X_("LTC"));

	get_vbox()->pack_start (name_hbox, false, false);
	get_vbox()->pack_start (type_hbox, false, false);

	add_button (_("Cancel"), RESPONSE_CANCEL);
	add_button (_("Add"), RESPONSE_ACCEPT);

	name_entry.show ();
	type_combo.show ();
	name_label.show ();
	type_label.show ();
	name_hbox.show ();
	type_hbox.show ();

	name_entry.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &Gtk::Dialog::response), Gtk::RESPONSE_ACCEPT));
}

string
TransportMastersWidget::AddTransportMasterDialog::get_name () const
{
	return name_entry.get_text ();
}

SyncSource
TransportMastersWidget::AddTransportMasterDialog::get_type() const
{
	string t = type_combo.get_active_text ();

	if (t == X_("MTC")) {
		return MTC;
	} else if (t == X_("MIDI Clock")) {
		return MIDIClock;
	}

	return LTC;
}

void
TransportMastersWidget::lost_sync_changed ()
{
	lost_sync_button.set_active (Config->get_transport_masters_just_roll_when_sync_lost());
}

void
TransportMastersWidget::lost_sync_button_toggled ()
{
	bool active = lost_sync_button.get_active ();
	Config->set_transport_masters_just_roll_when_sync_lost (active);
}

void
TransportMastersWidget::param_changed (string const & p)
{
	if (p == "transport-masters-just_roll-when-sync-lost") {
		lost_sync_changed ();
	} else if (p == "external-sync") {
		if (_session) {
			allow_master_select (!_session->config.get_external_sync());
		}
	}
}

void
TransportMastersWidget::allow_master_select (bool yn)
{
	for (vector<Row*>::iterator r = rows.begin(); r != rows.end(); ++r) {
		(*r)->use_button.set_sensitive (yn);
	}
}
