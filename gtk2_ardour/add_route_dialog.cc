/*
  Copyright (C) 2003 Paul Davis

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

#include <cstdio>
#include <cmath>

#include <sigc++/bind.h>
#include <gtkmm/stock.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/separator.h>
#include <gtkmm/table.h>

#include "pbd/error.h"
#include "pbd/convert.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/doi.h"

#include "widgets/tooltips.h"

#include "ardour/plugin_manager.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/vca.h"

#include "LuaBridge/LuaBridge.h"

#include "add_route_dialog.h"
#include "ardour_ui.h"
#include "route_group_dialog.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

std::vector<std::string> AddRouteDialog::channel_combo_strings;
std::vector<std::string> AddRouteDialog::builtin_types;

AddRouteDialog::AddRouteDialog ()
	: ArdourDialog (_("Add Track/Bus/VCA"))
	, routes_adjustment (1, 1, 128, 1, 4)
	, routes_spinner (routes_adjustment)
	, configuration_label (_("Configuration:"))
	, manual_label (_("Configuration:"))
	, add_label (_("Add:"))
	, name_label (_("Name:"))
	, group_label (_("Group:"))
	, insert_label (_("Position:"))
	, strict_io_label (_("Pin Mode:"))
	, mode_label (_("Record Mode:"))
	, instrument_label (_("Instrument:"))
	, name_edited_by_user (false)
{
	set_name ("AddRouteDialog");
	set_skip_taskbar_hint (true);
	set_resizable (false);
	set_position (WIN_POS_MOUSE);

	name_template_entry.set_name (X_("AddRouteDialogNameTemplateEntry"));
	// routes_spinner.set_name (X_("AddRouteDialogSpinner"));
	channel_combo.set_name (X_("ChannelCountSelector"));
	mode_combo.set_name (X_("ChannelCountSelector"));

	refill_track_modes ();

	if (builtin_types.empty()) {
		builtin_types.push_back (_("Audio Tracks"));
		builtin_types.push_back (_("MIDI Tracks"));
		builtin_types.push_back (_("Audio+MIDI Tracks"));
		builtin_types.push_back (_("Audio Busses"));
		builtin_types.push_back (_("MIDI Busses"));
		builtin_types.push_back (_("VCA Masters"));
	}

	insert_at_combo.append_text (_("First"));
	insert_at_combo.append_text (_("Before Selection"));
	insert_at_combo.append_text (_("After Selection"));
	insert_at_combo.append_text (_("Last"));
	insert_at_combo.set_active (3);

	strict_io_combo.append_text (_("Flexible-I/O"));
	strict_io_combo.append_text (_("Strict-I/O"));
	strict_io_combo.set_active (Config->get_strict_io () ? 1 : 0);

	//top-level VBox
	VBox* vbox = manage (new VBox);
	get_vbox()->set_spacing (4);
	vbox->set_spacing (18);
	vbox->set_border_width (5);

	//this box contains the template chooser, and the template details
	HBox* template_hbox = manage (new HBox);
	template_hbox->set_spacing (8);

	//scrollbars for the template chooser and template descriptions....
	Gtk::ScrolledWindow *template_scroller = manage (new Gtk::ScrolledWindow());
	template_scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	template_scroller->add (trk_template_chooser);

	Gtk::ScrolledWindow *desc_scroller = manage (new Gtk::ScrolledWindow());
	desc_scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	desc_scroller->add (trk_template_desc);

	//this is the outer frame that surrounds the description and the settings-table
	trk_template_outer_frame.set_name (X_("TextHighlightFrame"));

	//this is the "inner frame" that surrounds the description text
	trk_template_desc_frame.set_name (X_("TextHighlightFrame"));
	trk_template_desc_frame.add (*desc_scroller);

	/* template_chooser is the treeview showing available templates */
	trk_template_model = TreeStore::create (track_template_columns);
	trk_template_chooser.set_model (trk_template_model);
	trk_template_chooser.append_column (_("Template/Type"), track_template_columns.name);
#ifdef MIXBUS
	trk_template_chooser.append_column (_("Created With"), track_template_columns.created_with);
#endif
	trk_template_chooser.set_headers_visible (true);
	trk_template_chooser.get_selection()->set_mode (SELECTION_SINGLE);
	trk_template_chooser.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &AddRouteDialog::trk_template_row_selected));
	trk_template_chooser.set_sensitive (true);

	/* template_desc is the textview that displays the currently selected template's description */
	trk_template_desc.set_editable (false);
	trk_template_desc.set_can_focus (false);
	trk_template_desc.set_wrap_mode (Gtk::WRAP_WORD);
	trk_template_desc.set_size_request(400,200);
	trk_template_desc.set_name (X_("TextOnBackground"));
	trk_template_desc.set_border_width (6);

	Table *settings_table = manage (new Table (2, 6, false));
	settings_table->set_row_spacings (8);
	settings_table->set_col_spacings	(4);
	settings_table->set_col_spacing	(3, 20);
	settings_table->set_border_width	(12);

	VBox* settings_vbox = manage (new VBox);
	settings_vbox->pack_start(trk_template_desc_frame , true, true);
	settings_vbox->pack_start(*settings_table , true, true);
	settings_vbox->set_border_width	(4);

	trk_template_outer_frame.add (*settings_vbox);

	template_hbox->pack_start (*template_scroller, true, true);
	template_hbox->pack_start (trk_template_outer_frame, true, true);

	vbox->pack_start (*template_hbox, true, true);


	//Now pack the "settings table" with manual controls (these controls are sensitized by the left-side selection) 

	int n = 0;

	HBox *separator_hbox = manage (new HBox);
	separator_hbox->pack_start (manual_label, false, false);
	separator_hbox->pack_start (*(manage (new Gtk::HSeparator)), true, true);
	separator_hbox->set_spacing (6);
	settings_table->attach (*separator_hbox, 0, 6, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

	++n;

	// Number
	add_label.set_alignment (Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER);
	settings_table->attach (add_label, 0, 1, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);
	Gtk::Alignment *align = manage (new Alignment (0, .5, 0, 0));
	align->add (routes_spinner);
	settings_table->attach (*align, 1, 2, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

	++n;

	// Name
	name_label.set_alignment (Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER);
	settings_table->attach (name_label, 0, 1, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);
	settings_table->attach (name_template_entry, 1, 3, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

	// Route configuration
	configuration_label.set_alignment (Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER);
	settings_table->attach (configuration_label, 4, 5, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);
	settings_table->attach (channel_combo, 5, 6, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

	++n;

	// instrument choice (for MIDI)
	instrument_label.set_alignment (Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER);
	settings_table->attach (instrument_label, 0, 1, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);
	settings_table->attach (instrument_combo, 1, 3, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

	// Group choice
	group_label.set_alignment (Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER);
	settings_table->attach (group_label, 4, 5, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);
	settings_table->attach (route_group_combo, 5, 6, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

	++n;

	// New Route's I/O is.. {strict/flexible}
	if (Profile->get_mixbus ()) {
		strict_io_combo.set_active (1);
	} else {
		strict_io_label.set_alignment (Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER);
		settings_table->attach (strict_io_label, 0, 1, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);
		settings_table->attach (strict_io_combo, 1, 3, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

		ArdourWidgets::set_tooltip (strict_io_combo,
				_("With strict-i/o enabled, Effect Processors will not modify the number of channels on a track. The number of output channels will always match the number of input channels."));

		// recording mode
		mode_label.set_alignment (Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER);
		settings_table->attach (mode_label, 4, 5, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);
		settings_table->attach (mode_combo, 5, 6, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

		++n;
	}

	//now pack the "global" table at the bottom:  these controls are always sensitized for adding the selected item(s)

	Table *global_table = manage (new Table (2, 6, false));
	global_table->set_row_spacings (8);
	global_table->set_col_spacings	(4);
	global_table->set_border_width	(0);
	n = 0;

	global_table->attach (*(manage (new Gtk::HSeparator)), 0, 6, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);

	++n;

	// New route will be inserted at..
	insert_label.set_alignment (Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER);
	global_table->attach (insert_label, 0, 1, n, n + 1, Gtk::FILL, Gtk::SHRINK, 0, 0);
	global_table->attach (insert_at_combo, 1, 3, n, n + 1, Gtk::FILL, Gtk::FILL|Gtk::SHRINK, 0, 0);

//	++n;

	//quick-add button  (add item but don't close dialog)
	Gtk::Button* addnoclose_button = manage (new Gtk::Button(_("Add selected items (and leave dialog open)")));
	addnoclose_button->set_can_default ();
	addnoclose_button->signal_clicked ().connect (sigc::bind (sigc::mem_fun (*this, &Gtk::Dialog::response), Add));
	global_table->attach (*addnoclose_button, 4, 6, n, n + 1, Gtk::FILL, Gtk::FILL|Gtk::SHRINK, 0, 0);

	vbox->pack_start (*global_table, true, true);

	get_vbox()->pack_start (*vbox, false, false);

	name_template_entry.signal_insert_text ().connect (sigc::mem_fun (*this, &AddRouteDialog::name_template_entry_insertion));
	name_template_entry.signal_delete_text ().connect (sigc::mem_fun (*this, &AddRouteDialog::name_template_entry_deletion));
	channel_combo.signal_changed().connect (sigc::mem_fun (*this, &AddRouteDialog::channel_combo_changed));
	channel_combo.set_row_separator_func (sigc::mem_fun (*this, &AddRouteDialog::channel_separator));
	route_group_combo.set_row_separator_func (sigc::mem_fun (*this, &AddRouteDialog::route_separator));
	route_group_combo.signal_changed ().connect (sigc::mem_fun (*this, &AddRouteDialog::group_changed));

	show_all_children ();

	/* track template info will be managed whenever
	 * this dialog is shown, via ::on_show()
	 */

	add_button (_("Add and Close"), AddAndClose);
	set_response_sensitive (AddAndClose, true);
	set_default_response (AddAndClose);

	refill_channel_setups ();
}

AddRouteDialog::~AddRouteDialog ()
{
}

void
AddRouteDialog::on_response (int r)
{
	name_edited_by_user = false;
	/* Don't call ArdourDialog::on_response() because that will
	   automatically hide the dialog.
	*/
	Gtk::Dialog::on_response (r);
}

void
AddRouteDialog::trk_template_row_selected ()
{
	if (trk_template_chooser.get_selection()->count_selected_rows() != 1) {
		return;
	}

	TreeIter iter = trk_template_chooser.get_selection ()->get_selected ();
	assert (iter);

	string d = (*iter)[track_template_columns.description];
	trk_template_desc.get_buffer ()->set_text (d);

	const string n = (*iter)[track_template_columns.name];
	const string p = (*iter)[track_template_columns.path];

	if (p.substr (0, 11) == "urn:ardour:") {
		/* lua script - meta-template */
		const std::map<std::string, std::string> rs (ARDOUR_UI::instance()->route_setup_info (p.substr (11)));

		trk_template_desc.set_sensitive (true);

		add_label.set_sensitive (rs.find ("how_many") != rs.end ());
		name_label.set_sensitive (rs.find ("name") != rs.end());
		group_label.set_sensitive (rs.find ("group") != rs.end());
		configuration_label.set_sensitive (rs.find ("channels") != rs.end ());
		mode_label.set_sensitive (rs.find ("track_mode") != rs.end ());
		instrument_label.set_sensitive (rs.find ("instrument") != rs.end ());
		strict_io_label.set_sensitive (rs.find ("strict_io") != rs.end());

		routes_spinner.set_sensitive (rs.find ("how_many") != rs.end ());
		name_template_entry.set_sensitive (rs.find ("name") != rs.end ());
		route_group_combo.set_sensitive (rs.find ("group") != rs.end());
		channel_combo.set_sensitive (rs.find ("channels") != rs.end ());
		mode_combo.set_sensitive (rs.find ("track_mode") != rs.end ());
		instrument_combo.set_sensitive (rs.find ("instrument") != rs.end ());
		strict_io_combo.set_sensitive (rs.find ("strict_io") != rs.end());

		bool any_enabled = rs.find ("how_many") != rs.end ()
			|| rs.find ("name") != rs.end ()
			|| rs.find ("group") != rs.end()
			|| rs.find ("channels") != rs.end ()
			|| rs.find ("track_mode") != rs.end ()
			|| rs.find ("instrument") != rs.end ()
			|| rs.find ("strict_io") != rs.end();

		manual_label.set_sensitive (any_enabled);

		std::map<string,string>::const_iterator it;

		if ((it = rs.find ("name")) != rs.end()) {
			name_template_entry.set_text (it->second);
		}

		if ((it = rs.find ("how_many")) != rs.end()) {
			if (atoi (it->second.c_str()) > 0) {
				routes_adjustment.set_value (atoi (it->second.c_str()));
			}
		}

		if ((it = rs.find ("track_mode")) != rs.end()) {
			switch ((ARDOUR::TrackMode) atoi (it->second.c_str())) {
				case ARDOUR::Normal:
					mode_combo.set_active_text (_("Normal"));
					break;
				case ARDOUR::Destructive:
					if (!ARDOUR::Profile->get_mixbus ()) {
						mode_combo.set_active_text (_("Tape"));
					}
					break;
				default: // "NonLayered" enum is still present for session-format compat
					break;
			}
		}

		if ((it = rs.find ("strict_io")) != rs.end()) {
			if (it->second == X_("true")) {
				strict_io_combo.set_active (1);
			} else if (it->second == X_("false")) {
				strict_io_combo.set_active (0);
			}
		}

		if ((it = rs.find ("channels")) != rs.end()) {
			uint32_t channels = atoi (it->second.c_str());
			for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
				if ((*i).channels == channels) {
					channel_combo.set_active_text ((*i).name);
					break;
				}
			}
		}

	} else if (!p.empty ()) {
		/* user-template */
		trk_template_desc.set_sensitive (true);

		manual_label.set_sensitive (true);
		add_label.set_sensitive (true);
		name_label.set_sensitive (true);
		group_label.set_sensitive (false);
		strict_io_label.set_sensitive (false);
		configuration_label.set_sensitive (false);
		mode_label.set_sensitive (false);
		instrument_label.set_sensitive (false);

		routes_spinner.set_sensitive (true);
		name_template_entry.set_sensitive (true);
		channel_combo.set_sensitive (false);
		mode_combo.set_sensitive (false);
		instrument_combo.set_sensitive (false);
		strict_io_combo.set_sensitive (false);
		route_group_combo.set_sensitive (false);

	} else {
		/* all manual mode */
		trk_template_desc.set_sensitive (false);

		manual_label.set_sensitive (true);
		add_label.set_sensitive (true);
		name_label.set_sensitive (true);
		group_label.set_sensitive (true);
		strict_io_label.set_sensitive (true);

		routes_spinner.set_sensitive (true);
		name_template_entry.set_sensitive (true);
		track_type_chosen ();
	}
}


void
AddRouteDialog::name_template_entry_insertion (Glib::ustring const &,int*)
{
	name_edited_by_user = true;
}

void
AddRouteDialog::name_template_entry_deletion (int, int)
{
	name_edited_by_user = true;
}

void
AddRouteDialog::channel_combo_changed ()
{
	refill_track_modes ();
}

std::string
AddRouteDialog::get_template_path ()
{
	string p;
	
	if (trk_template_chooser.get_selection()->count_selected_rows() > 0) {
		TreeIter iter = trk_template_chooser.get_selection()->get_selected();

		if (iter) {
			string n = (*iter)[track_template_columns.name];
			if ( n != _("Manual Configuration") ) {
				p = (*iter)[track_template_columns.path];
			}
		}
	}

	return p;
}


AddRouteDialog::TypeWanted
AddRouteDialog::type_wanted()
{
	if (trk_template_chooser.get_selection()->count_selected_rows() != 1) {
		return AudioTrack;
	}
	TreeIter iter = trk_template_chooser.get_selection ()->get_selected ();
	assert (iter);

	const string str = (*iter)[track_template_columns.name];
	if (str == _("Audio Busses")) {
		return AudioBus;
	} else if (str == _("MIDI Busses")){
		return MidiBus;
	} else if (str == _("MIDI Tracks")){
		return MidiTrack;
	} else if (str == _("Audio+MIDI Tracks")) {
		return MixedTrack;
	} else if (str == _("Audio Tracks")) {
		return AudioTrack;
	} else if (str == _("VCA Masters")) {
		return VCAMaster;
	} else {
		assert (0);
		return AudioTrack;
	}
}

void
AddRouteDialog::maybe_update_name_template_entry ()
{
	if (name_edited_by_user) {
		return;
	}

	switch (type_wanted()) {
	case AudioTrack:
		name_template_entry.set_text (_("Audio"));
		break;
	case MidiTrack:
		name_template_entry.set_text (_("MIDI"));
		break;
	case MixedTrack:
		name_template_entry.set_text (_("Audio+MIDI"));
		break;
	case AudioBus:
	case MidiBus:
		name_template_entry.set_text (_("Bus"));
		break;
	case VCAMaster:
		name_template_entry.set_text (VCA::default_name_template());
		break;
	}
	name_edited_by_user = false;
}

void
AddRouteDialog::track_type_chosen ()
{
	switch (type_wanted()) {
	case AudioTrack:
		mode_combo.set_sensitive (true);
		channel_combo.set_sensitive (true);
		instrument_combo.set_sensitive (false);
		configuration_label.set_sensitive (true);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (false);
		route_group_combo.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	case MidiTrack:
		channel_combo.set_sensitive (false);
		mode_combo.set_sensitive (false);
		instrument_combo.set_sensitive (true);
		configuration_label.set_sensitive (false);
		mode_label.set_sensitive (false);
		instrument_label.set_sensitive (true);
		route_group_combo.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	case MixedTrack:
		{
			MessageDialog msg (_("Audio+MIDI tracks are intended for use <b>ONLY</b> with plugins that use both audio and MIDI input data\n\n"
					     "If you do not plan to use such a plugin, then use a normal audio or MIDI track instead."),
					   true, MESSAGE_INFO, BUTTONS_OK, true);
			msg.set_position (WIN_POS_MOUSE);
			msg.run ();
		}
		channel_combo.set_sensitive (true);
		mode_combo.set_sensitive (true);
		instrument_combo.set_sensitive (true);
		configuration_label.set_sensitive (true);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (true);
		route_group_combo.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	case AudioBus:
		mode_combo.set_sensitive (false);
		channel_combo.set_sensitive (true);
		instrument_combo.set_sensitive (false);
		configuration_label.set_sensitive (true);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (false);
		route_group_combo.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	case VCAMaster:
		mode_combo.set_sensitive (false);
		channel_combo.set_sensitive (false);
		instrument_combo.set_sensitive (false);
		configuration_label.set_sensitive (false);
		mode_label.set_sensitive (false);
		instrument_label.set_sensitive (false);
		route_group_combo.set_sensitive (false);
		strict_io_combo.set_sensitive (false);
		insert_at_combo.set_sensitive (false);
		break;
	case MidiBus:
		mode_combo.set_sensitive (false);
		channel_combo.set_sensitive (false);
		instrument_combo.set_sensitive (true);
		configuration_label.set_sensitive (false);
		mode_label.set_sensitive (true);
		instrument_label.set_sensitive (true);
		strict_io_combo.set_sensitive (true);
		insert_at_combo.set_sensitive (true);
		break;
	}

	maybe_update_name_template_entry ();
}

string
AddRouteDialog::name_template () const
{
	return name_template_entry.get_text ();
}

bool
AddRouteDialog::name_template_is_default () const
{
	string n = name_template();

	if (n == _("Audio") ||
	    n == _("MIDI") ||
	    n == _("Audio+MIDI") ||
	    n == _("Bus") ||
	    n == VCA::default_name_template()) {
		return true;
	}

	return false;
}

int
AddRouteDialog::count ()
{
	return (int) floor (routes_adjustment.get_value ());
}

void
AddRouteDialog::refill_track_modes ()
{
	vector<string> s;

	s.push_back (_("Normal"));
	if (!ARDOUR::Profile->get_mixbus ()) {
		s.push_back (_("Tape"));
	}

	set_popdown_strings (mode_combo, s);
	mode_combo.set_active_text (s.front());
}

ARDOUR::TrackMode
AddRouteDialog::mode ()
{
	std::string str = mode_combo.get_active_text();
	if (str == _("Normal")) {
		return ARDOUR::Normal;
	} else if (str == _("Non Layered")){
		return ARDOUR::NonLayered;
	} else if (str == _("Tape")) {
		return ARDOUR::Destructive;
	} else {
		fatal << string_compose (X_("programming error: unknown track mode in add route dialog combo = %1"), str)
		      << endmsg;
		abort(); /*NOTREACHED*/
	}
	/* keep gcc happy */
	return ARDOUR::Normal;
}

uint32_t
AddRouteDialog::channel_count ()
{
	string str = channel_combo.get_active_text();
	for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
		if (str == (*i).name) {
			return (*i).channels;
		}
	}
	return 0;
}

ChanCount
AddRouteDialog::channels ()
{
	ChanCount ret;
	switch (type_wanted()) {
	case AudioTrack:
	case AudioBus:
		ret.set (DataType::AUDIO, channel_count ());
		ret.set (DataType::MIDI, 0);
		break;

	case MidiBus:
	case MidiTrack:
		ret.set (DataType::AUDIO, 0);
		ret.set (DataType::MIDI, 1);
		break;

	case MixedTrack:
		ret.set (DataType::AUDIO, channel_count ());
		ret.set (DataType::MIDI, 1);
		break;
	default:
		break;
	}

	return ret;
}

void
AddRouteDialog::on_show ()
{
	routes_spinner.grab_focus ();
	name_edited_by_user = false;

	refill_route_groups ();

	Dialog::on_show ();
}

void
AddRouteDialog::refill_channel_setups ()
{
	ChannelSetup chn;

	string channel_current_choice = channel_combo.get_active_text();

	channel_combo_strings.clear ();
	channel_setups.clear ();

	chn.name = _("Mono");
	chn.channels = 1;
	channel_setups.push_back (chn);

	chn.name = _("Stereo");
	chn.channels = 2;
	channel_setups.push_back (chn);

	if (!ARDOUR::Profile->get_mixbus()) {

		chn.name = "separator";
		channel_setups.push_back (chn);

		chn.name = _("3 Channel");
		chn.channels = 3;
		channel_setups.push_back (chn);

		chn.name = _("4 Channel");
		chn.channels = 4;
		channel_setups.push_back (chn);

		chn.name = _("5 Channel");
		chn.channels = 5;
		channel_setups.push_back (chn);

		chn.name = _("6 Channel");
		chn.channels = 6;
		channel_setups.push_back (chn);

		chn.name = _("8 Channel");
		chn.channels = 8;
		channel_setups.push_back (chn);

		chn.name = _("12 Channel");
		chn.channels = 12;
		channel_setups.push_back (chn);

		chn.name = _("Custom");
		chn.channels = 0;
		channel_setups.push_back (chn);
	}

	for (ChannelSetups::iterator i = channel_setups.begin(); i != channel_setups.end(); ++i) {
		channel_combo_strings.push_back ((*i).name);
	}

	trk_template_model->clear();
	bool selected_default = false;

	for (std::vector<std::string>::const_iterator i = builtin_types.begin(); i != builtin_types.end(); ++i) {
		TreeModel::Row row = *(trk_template_model->append ());
		row[track_template_columns.name] = (*i);
		row[track_template_columns.path] = "";
		row[track_template_columns.description] = _("Use the controls below");
		row[track_template_columns.created_with] = "";

		if (!selected_default && !Profile->get_mixbus ()) {
			trk_template_chooser.get_selection()->select(row);
			selected_default = true;
		}
	}

	/* Add any Lua scripts (factory templates) found in the scripts folder */
	LuaScriptList& ms (LuaScripting::instance ().scripts (LuaScriptInfo::EditorAction));
	for (LuaScriptList::const_iterator s = ms.begin(); s != ms.end(); ++s) {
		if (!((*s)->subtype & LuaScriptInfo::RouteSetup)) {
			continue;
		}
		TreeModel::Row row;
		if ( (*s)->name == "Create Audio Tracks Interactively" && Profile->get_mixbus ()) {
			// somewhat-special, Ben says: "most-used template"
			row = *(trk_template_model->prepend ());
		} else {
			row = *(trk_template_model->append ());
		}
		row[track_template_columns.name] = (*s)->name;
		row[track_template_columns.path] = "urn:ardour:" + (*s)->path;
		row[track_template_columns.description] = (*s)->description;
		row[track_template_columns.created_with] = _("{Factory Template}");

		if ((*s)->name == "Create Audio Tracks Interactively" && Profile->get_mixbus ()) {
			trk_template_chooser.get_selection()->select(row);
			selected_default = true;
		}
	}

	std::vector<ARDOUR::TemplateInfo> route_templates;
	ARDOUR::find_route_templates (route_templates);

	for (vector<TemplateInfo>::iterator x = route_templates.begin(); x != route_templates.end(); ++x) {
		TreeModel::Row row = *(trk_template_model->append ());

		row[track_template_columns.name] = x->name;
		row[track_template_columns.path] = x->path;
		row[track_template_columns.description] = x->description;
		row[track_template_columns.created_with] = x->created_with;
	}

	set_popdown_strings (channel_combo, channel_combo_strings);

	if (!channel_current_choice.empty()) {
		channel_combo.set_active_text (channel_current_choice);
	} else {
		channel_combo.set_active_text (channel_combo_strings.front());
	}
}

void
AddRouteDialog::add_route_group (RouteGroup* g)
{
	route_group_combo.insert_text (3, g->name ());
}

RouteGroup*
AddRouteDialog::route_group ()
{
	if (!_session || route_group_combo.get_active_row_number () == 2) {
		return 0;
	}

	return _session->route_group_by_name (route_group_combo.get_active_text());
}

bool
AddRouteDialog::use_strict_io() {
	return strict_io_combo.get_active_row_number () == 1;
}

void
AddRouteDialog::refill_route_groups ()
{
	route_group_combo.clear ();
	route_group_combo.append_text (_("New Group..."));

	route_group_combo.append_text ("separator");

	route_group_combo.append_text (_("No Group"));

	if (_session) {
		_session->foreach_route_group (sigc::mem_fun (*this, &AddRouteDialog::add_route_group));
	}

	route_group_combo.set_active (2);
}

void
AddRouteDialog::group_changed ()
{
	if (_session && route_group_combo.get_active_text () == _("New Group...")) {
		RouteGroup* g = new RouteGroup (*_session, "");
		RouteGroupDialog* d = new RouteGroupDialog (g, true);

		d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &AddRouteDialog::new_group_dialog_finished), d));
		d->present();
	}
}

void
AddRouteDialog::new_group_dialog_finished (int r, RouteGroupDialog* d)
{
	if (r == RESPONSE_OK) {

		if (!d->name_check()) {
			return;
		}

		if (_session) {
			_session->add_route_group (d->group());
		}

		add_route_group (d->group());
		route_group_combo.set_active (3);
	} else {
		delete d->group ();
		route_group_combo.set_active (2);
	}

	delete_when_idle (d);
}

RouteDialogs::InsertAt
AddRouteDialog::insert_at ()
{
	using namespace RouteDialogs;

	std::string str = insert_at_combo.get_active_text();

	if (str == _("First")) {
		return First;
	} else if (str == _("After Selection")) {
		return AfterSelection;
	} else if (str == _("Before Selection")){
		return BeforeSelection;
	}
	return Last;
}

bool
AddRouteDialog::channel_separator (const Glib::RefPtr<Gtk::TreeModel> &, const Gtk::TreeModel::iterator &i)
{
	channel_combo.set_active (i);

	return channel_combo.get_active_text () == "separator";
}

bool
AddRouteDialog::route_separator (const Glib::RefPtr<Gtk::TreeModel> &, const Gtk::TreeModel::iterator &i)
{
	route_group_combo.set_active (i);

	return route_group_combo.get_active_text () == "separator";
}

PluginInfoPtr
AddRouteDialog::requested_instrument ()
{
	return instrument_combo.selected_instrument();
}
