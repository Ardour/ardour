/*
    Copyright (C) 2000 Paul Davis

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

#include <cstdlib>
#include <cmath>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/ffs.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"
#include "pbd/basename.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/selector.h"
#include "gtkmm2ext/bindable_button.h"
#include "gtkmm2ext/utils.h"

#include "ardour/event_type_map.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/playlist.h"
#include "ardour/plugin_insert.h"
#include "ardour/profile.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_object.h"
#include "ardour/source.h"
#include "ardour/track.h"
#include "ardour/types.h"

#include "ardour_button.h"
#include "automation_line.h"
#include "automation_time_axis.h"
#include "editor.h"
#include "enums.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_channel_selector.h"
#include "midi_scroomer.h"
#include "midi_streamview.h"
#include "midi_region_view.h"
#include "midi_time_axis.h"
#include "piano_roll_header.h"
#include "playlist_selector.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "point_selection.h"
#include "prompter.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "step_editor.h"
#include "tooltips.h"
#include "utils.h"
#include "note_base.h"

#include "ardour/midi_track.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using namespace std;

// Minimum height at which a control is displayed
static const uint32_t MIDI_CONTROLS_BOX_MIN_HEIGHT = 160;
static const uint32_t KEYBOARD_MIN_HEIGHT = 130;

MidiTimeAxisView::MidiTimeAxisView (PublicEditor& ed, Session* sess, ArdourCanvas::Canvas& canvas)
	: SessionHandlePtr (sess)
	, RouteTimeAxisView (ed, sess, canvas)
	, _ignore_signals(false)
	, _range_scroomer(0)
	, _piano_roll_header(0)
	, _note_mode(Sustained)
	, _note_mode_item(0)
	, _percussion_mode_item(0)
	, _color_mode(MeterColors)
	, _meter_color_mode_item(0)
	, _channel_color_mode_item(0)
	, _track_color_mode_item(0)
	, _channel_selector (0)
	, _step_edit_item (0)
	, controller_menu (0)
	, poly_pressure_menu (0)
	, _step_editor (0)
{
	_midnam_model_selector.disable_scrolling();
	_midnam_custom_device_mode_selector.disable_scrolling();
}

void
MidiTimeAxisView::set_note_highlight (uint8_t note) {
	_piano_roll_header->set_note_highlight (note);
}

void
MidiTimeAxisView::set_route (boost::shared_ptr<Route> rt)
{
	_route = rt;

	_view = new MidiStreamView (*this);

	if (is_track ()) {
		_piano_roll_header = new PianoRollHeader(*midi_view());
		_range_scroomer = new MidiScroomer(midi_view()->note_range_adjustment);
		_range_scroomer->DoubleClicked.connect (
			sigc::bind (sigc::mem_fun(*this, &MidiTimeAxisView::set_note_range),
			            MidiStreamView::ContentsRange, false));
	}

	/* This next call will result in our height being set up, so it must come after
	   the creation of the piano roll / range scroomer as their visibility is set up
	   when our height is.
	*/
	RouteTimeAxisView::set_route (rt);

	_view->apply_color (gdk_color_to_rgba (color()), StreamView::RegionColor);

	subplugin_menu.set_name ("ArdourContextMenu");

	if (!gui_property ("note-range-min").empty ()) {
		midi_view()->apply_note_range (atoi (gui_property ("note-range-min").c_str()),
		                               atoi (gui_property ("note-range-max").c_str()),
		                               true);
	}

	_view->ContentsHeightChanged.connect (
		sigc::mem_fun (*this, &MidiTimeAxisView::contents_height_changed));

	ignore_toggle = false;

	if (is_midi_track()) {
		_note_mode = midi_track()->note_mode();
	}

	/* if set_state above didn't create a gain automation child, we need to make one */
	if (automation_child (GainAutomation) == 0) {
		create_automation_child (GainAutomation, false);
	}

	/* if set_state above didn't create a mute automation child, we need to make one */
	if (automation_child (MuteAutomation) == 0) {
		create_automation_child (MuteAutomation, false);
	}

	if (_route->panner_shell()) {
		_route->panner_shell()->Changed.connect (*this, invalidator (*this), boost::bind (&MidiTimeAxisView::ensure_pan_views, this, false), gui_context());
	}

	/* map current state of the route */
	ensure_pan_views (false);
	update_control_names();
	processors_changed (RouteProcessorChange ());

	_route->processors_changed.connect (*this, invalidator (*this),
	                                    boost::bind (&MidiTimeAxisView::processors_changed, this, _1),
	                                    gui_context());

	if (is_track()) {
		_piano_roll_header->SetNoteSelection.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::set_note_selection));
		_piano_roll_header->AddNoteSelection.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::add_note_selection));
		_piano_roll_header->ExtendNoteSelection.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::extend_note_selection));
		_piano_roll_header->ToggleNoteSelection.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::toggle_note_selection));

		/* Update StreamView during scroomer drags.*/

		_range_scroomer->DragStarting.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::start_scroomer_update));
		_range_scroomer->DragFinishing.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::stop_scroomer_update));

		/* Put the scroomer and the keyboard in a VBox with a padding
		   label so that they can be reduced in height for stacked-view
		   tracks.
		*/

		HSeparator* separator = manage (new HSeparator());
		separator->set_name("TrackSeparator");
		separator->set_size_request(-1, 1);
		separator->show();

		VBox* v = manage (new VBox);
		HBox* h = manage (new HBox);
		h->pack_end (*_piano_roll_header);
		h->pack_end (*_range_scroomer);
		v->pack_start (*separator, false, false);
		v->pack_start (*h, true, true);
		v->show ();
		h->show ();
		top_hbox.remove(scroomer_placeholder);
		time_axis_hbox.pack_end(*v, false, false, 0);
		midi_scroomer_size_group->add_widget (*v);

		midi_view()->NoteRangeChanged.connect (
			sigc::mem_fun(*this, &MidiTimeAxisView::update_range));

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (
			sigc::mem_fun(*this, &MidiTimeAxisView::region_view_added));

		if (!_editor.have_idled()) {
			/* first idle will do what we need */
		} else {
			first_idle ();
		}
	}

	if (gui_property (X_("midnam-model-name")).empty()) {
		set_gui_property (X_("midnam-model-name"), "Generic");
	}

	if (gui_property (X_("midnam-custom-device-mode")).empty()) {
		boost::shared_ptr<MIDI::Name::MasterDeviceNames> device_names = get_device_names();
		if (device_names) {
			set_gui_property (X_("midnam-custom-device-mode"),
			                  *device_names->custom_device_mode_names().begin());
		}
	}

	set_tooltip (_midnam_model_selector, _("External MIDI Device"));
	set_tooltip (_midnam_custom_device_mode_selector, _("External Device Mode"));

	_midi_controls_box.pack_start (_midnam_model_selector, false, false, 2);
	_midi_controls_box.pack_start (_midnam_custom_device_mode_selector, false, false, 2);

	_midi_controls_box.set_homogeneous(false);
	_midi_controls_box.set_border_width (2);

	MIDI::Name::MidiPatchManager::instance().PatchesChanged.connect (*this, invalidator (*this),
			boost::bind (&MidiTimeAxisView::setup_midnam_patches, this),
			gui_context());

	setup_midnam_patches ();
	update_patch_selector ();

	model_changed (gui_property(X_("midnam-model-name")));
	custom_device_mode_changed (gui_property(X_("midnam-custom-device-mode")));

	controls_vbox.pack_start(_midi_controls_box, false, false);

	const string color_mode = gui_property ("color-mode");
	if (!color_mode.empty()) {
		_color_mode = ColorMode (string_2_enum(color_mode, _color_mode));
		if (_channel_selector && _color_mode == ChannelColors) {
			_channel_selector->set_channel_colors(NoteBase::midi_channel_colors);
		}
	}

	set_color_mode (_color_mode, true, false);

	const string note_mode = gui_property ("note-mode");
	if (!note_mode.empty()) {
		_note_mode = NoteMode (string_2_enum (note_mode, _note_mode));
		if (_percussion_mode_item) {
			_percussion_mode_item->set_active (_note_mode == Percussive);
		}
	}

	/* Look for any GUI object state nodes that represent automation children
	 * that should exist, and create the children.
	 */

	const list<string> gui_ids = gui_object_state().all_ids ();
	for (list<string>::const_iterator i = gui_ids.begin(); i != gui_ids.end(); ++i) {
		PBD::ID route_id;
		bool has_parameter;
		Evoral::Parameter parameter (0, 0, 0);

		bool const p = AutomationTimeAxisView::parse_state_id (
			*i, route_id, has_parameter, parameter);
		if (p && route_id == _route->id () && has_parameter) {
			const std::string& visible = gui_object_state().get_string (*i, X_("visible"));
			create_automation_child (parameter, string_is_affirmative (visible));
		}
	}
}

void
MidiTimeAxisView::processors_changed (RouteProcessorChange c)
{
	RouteTimeAxisView::processors_changed (c);
	update_patch_selector ();
}

void
MidiTimeAxisView::first_idle ()
{
	if (is_track ()) {
		_view->attach ();
	}
}

MidiTimeAxisView::~MidiTimeAxisView ()
{
	delete _channel_selector;

	delete _piano_roll_header;
	_piano_roll_header = 0;

	delete _range_scroomer;
	_range_scroomer = 0;

	delete controller_menu;
	delete _step_editor;
}

void
MidiTimeAxisView::check_step_edit ()
{
	ensure_step_editor ();
	_step_editor->check_step_edit ();
}

void
MidiTimeAxisView::setup_midnam_patches ()
{
	typedef MIDI::Name::MidiPatchManager PatchManager;
	PatchManager& patch_manager = PatchManager::instance();

	_midnam_model_selector.clear_items ();
	for (PatchManager::DeviceNamesByMaker::const_iterator m = patch_manager.devices_by_manufacturer().begin();
			m != patch_manager.devices_by_manufacturer().end(); ++m) {
		Menu*                   menu  = Gtk::manage(new Menu);
		Menu_Helpers::MenuList& items = menu->items();

		// Build manufacturer submenu
		for (MIDI::Name::MIDINameDocument::MasterDeviceNamesList::const_iterator n = m->second.begin();
				n != m->second.end(); ++n) {
			Menu_Helpers::MenuElem elem = Gtk::Menu_Helpers::MenuElem(
					n->first.c_str(),
					sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::model_changed),
						n->first.c_str()));

			items.push_back(elem);
		}

		// Add manufacturer submenu to selector
		_midnam_model_selector.AddMenuElem(Menu_Helpers::MenuElem(m->first, *menu));
	}

	if (!get_device_names()) {
		model_changed ("Generic");
	}
}

void
MidiTimeAxisView::drop_instrument_ref ()
{
	midnam_connection.drop_connections ();
}
void
MidiTimeAxisView::start_scroomer_update ()
{
	_note_range_changed_connection.disconnect();
	_note_range_changed_connection = midi_view()->NoteRangeChanged.connect (
		sigc::mem_fun (*this, &MidiTimeAxisView::note_range_changed));
}
void
MidiTimeAxisView::stop_scroomer_update ()
{
	_note_range_changed_connection.disconnect();
}

void
MidiTimeAxisView::update_patch_selector ()
{
	typedef MIDI::Name::MidiPatchManager PatchManager;
	PatchManager& patch_manager = PatchManager::instance();

	bool pluginprovided = false;
	if (_route) {
		boost::shared_ptr<Processor> the_instrument (_route->the_instrument());
		boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert>(the_instrument);
		if (pi && pi->plugin ()->has_midnam ()) {
			midnam_connection.drop_connections ();
			the_instrument->DropReferences.connect (midnam_connection, invalidator (*this),
					boost::bind (&MidiTimeAxisView::drop_instrument_ref, this),
					gui_context());
			pi->plugin()->UpdateMidnam.connect (midnam_connection, invalidator (*this),
					boost::bind (&Plugin::read_midnam, pi->plugin ()),
					gui_context());

			pluginprovided = true;
			std::string model_name = pi->plugin ()->midnam_model ();
			if (gui_property (X_("midnam-model-name")) != model_name) {
				model_changed (model_name);
			}
		}
	}

	if (patch_manager.all_models().empty() || pluginprovided) {
		_midnam_model_selector.hide ();
		_midnam_custom_device_mode_selector.hide ();
	} else {
		_midnam_model_selector.show ();
		_midnam_custom_device_mode_selector.show ();
	}
}

void
MidiTimeAxisView::model_changed(const std::string& model)
{
	set_gui_property (X_("midnam-model-name"), model);

	const std::list<std::string> device_modes = MIDI::Name::MidiPatchManager::instance()
		.custom_device_mode_names_by_model(model);

	_midnam_model_selector.set_text(model);
	_midnam_custom_device_mode_selector.clear_items();

	for (std::list<std::string>::const_iterator i = device_modes.begin();
	     i != device_modes.end(); ++i) {
		_midnam_custom_device_mode_selector.AddMenuElem(
			Gtk::Menu_Helpers::MenuElem(
				*i, sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::custom_device_mode_changed),
				               *i)));
	}

	if (!device_modes.empty()) {
		custom_device_mode_changed(device_modes.front());
	}

	if (device_modes.size() > 1) {
		_midnam_custom_device_mode_selector.show();
	} else {
		_midnam_custom_device_mode_selector.hide();
	}

	// now this is a real bad hack
	if (device_modes.size() > 0) {
		_route->instrument_info().set_external_instrument (model, device_modes.front());
	} else {
		_route->instrument_info().set_external_instrument (model, "");
	}

	// Rebuild controller menu
	_controller_menu_map.clear ();
	delete controller_menu;
	controller_menu = 0;
	build_automation_action_menu(false);
}

void
MidiTimeAxisView::custom_device_mode_changed(const std::string& mode)
{
	const std::string model = gui_property (X_("midnam-model-name"));

	set_gui_property (X_("midnam-custom-device-mode"), mode);
	_midnam_custom_device_mode_selector.set_text(mode);
	_route->instrument_info().set_external_instrument (model, mode);
}

MidiStreamView*
MidiTimeAxisView::midi_view()
{
	return dynamic_cast<MidiStreamView*>(_view);
}

void
MidiTimeAxisView::set_height (uint32_t h, TrackHeightMode m)
{
	if (h >= MIDI_CONTROLS_BOX_MIN_HEIGHT) {
		_midi_controls_box.show ();
	} else {
		_midi_controls_box.hide();
	}

	if (h >= KEYBOARD_MIN_HEIGHT) {
		if (is_track() && _range_scroomer) {
			_range_scroomer->show();
		}
		if (is_track() && _piano_roll_header) {
			_piano_roll_header->show();
		}
	} else {
		if (is_track() && _range_scroomer) {
			_range_scroomer->hide();
		}
		if (is_track() && _piano_roll_header) {
			_piano_roll_header->hide();
		}
	}

	/* We need to do this after changing visibility of our stuff, as it will
	   eventually trigger a call to Editor::reset_controls_layout_width(),
	   which needs to know if we have just shown or hidden a scroomer /
	   piano roll.
	*/
	RouteTimeAxisView::set_height (h, m);
}

void
MidiTimeAxisView::append_extra_display_menu_items ()
{
	using namespace Menu_Helpers;

	MenuList& items = display_menu->items();

	// Note range
	Menu *range_menu = manage(new Menu);
	MenuList& range_items = range_menu->items();
	range_menu->set_name ("ArdourContextMenu");

	range_items.push_back (
		MenuElem (_("Show Full Range"),
		          sigc::bind (sigc::mem_fun(*this, &MidiTimeAxisView::set_note_range),
		                      MidiStreamView::FullRange, true)));

	range_items.push_back (
		MenuElem (_("Fit Contents"),
		          sigc::bind (sigc::mem_fun(*this, &MidiTimeAxisView::set_note_range),
		                      MidiStreamView::ContentsRange, true)));

	items.push_back (MenuElem (_("Note Range"), *range_menu));
	items.push_back (MenuElem (_("Note Mode"), *build_note_mode_menu()));
	items.push_back (MenuElem (_("Channel Selector"),
				   sigc::mem_fun(*this, &MidiTimeAxisView::toggle_channel_selector)));

	items.push_back (MenuElem (_("Select Patch"), *build_patch_menu()));

	color_mode_menu = build_color_mode_menu();
	if (color_mode_menu) {
		items.push_back (MenuElem (_("Color Mode"), *color_mode_menu));
	}

	items.push_back (SeparatorElem ());
}

void
MidiTimeAxisView::toggle_channel_selector ()
{
	if (!_channel_selector) {
		_channel_selector = new MidiChannelSelectorWindow (midi_track());

		if (_color_mode == ChannelColors) {
			_channel_selector->set_channel_colors(NoteBase::midi_channel_colors);
		} else {
			_channel_selector->set_default_channel_color ();
		}

		_channel_selector->show_all ();
	} else {
		_channel_selector->cycle_visibility ();
	}
}

void
MidiTimeAxisView::build_automation_action_menu (bool for_selection)
{
	using namespace Menu_Helpers;

	/* If we have a controller menu, we need to detach it before
	   RouteTimeAxis::build_automation_action_menu destroys the
	   menu it is attached to.  Otherwise GTK destroys
	   controller_menu's gobj, meaning that it can't be reattached
	   below.  See bug #3134.
	*/

	if (controller_menu) {
		detach_menu (*controller_menu);
	}

	_channel_command_menu_map.clear ();
	RouteTimeAxisView::build_automation_action_menu (for_selection);

	MenuList& automation_items = automation_action_menu->items();

	uint16_t selected_channels = midi_track()->get_playback_channel_mask();

	if (selected_channels !=  0) {

		automation_items.push_back (SeparatorElem());

		/* these 2 MIDI "command" types are semantically more like automation
		   than note data, but they are not MIDI controllers. We give them
		   special status in this menu, since they will not show up in the
		   controller list and anyone who actually knows something about MIDI
		   (!) would not expect to find them there.
		*/

		add_channel_command_menu_item (
			automation_items, _("Bender"), MidiPitchBenderAutomation, 0);
		automation_items.back().set_sensitive (
			!for_selection || _editor.get_selection().tracks.size() == 1);
		add_channel_command_menu_item (
			automation_items, _("Pressure"), MidiChannelPressureAutomation, 0);
		automation_items.back().set_sensitive (
			!for_selection || _editor.get_selection().tracks.size() == 1);

		/* now all MIDI controllers. Always offer the possibility that we will
		   rebuild the controllers menu since it might need to be updated after
		   a channel mode change or other change. Also detach it first in case
		   it has been used anywhere else.
		*/

		build_controller_menu ();

		automation_items.push_back (MenuElem (_("Controllers"), *controller_menu));

		if (!poly_pressure_menu) {
			poly_pressure_menu = new Gtk::Menu;
		}

		automation_items.push_back (MenuElem  (_("Polyphonic Pressure"), *poly_pressure_menu));

		automation_items.back().set_sensitive (
			!for_selection || _editor.get_selection().tracks.size() == 1);
	} else {
		automation_items.push_back (
			MenuElem (string_compose ("<i>%1</i>", _("No MIDI Channels selected"))));
		dynamic_cast<Label*> (automation_items.back().get_child())->set_use_markup (true);
	}
}

void
MidiTimeAxisView::change_all_channel_tracks_visibility (bool yn, Evoral::Parameter param)
{
	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (param.type(), chn, param.id());
			Gtk::CheckMenuItem* menu = automation_child_menu_item (fully_qualified_param);

			if (menu) {
				menu->set_active (yn);
			}
		}
	}
}

void
MidiTimeAxisView::add_channel_command_menu_item (Menu_Helpers::MenuList& items,
                                                 const string&           label,
                                                 AutomationType          auto_type,
                                                 uint8_t                 cmd)
{
	using namespace Menu_Helpers;

	/* count the number of selected channels because we will build a different menu
	   structure if there is more than 1 selected.
	 */

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();
	int chn_cnt = 0;

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {
			if (++chn_cnt > 1) {
				break;
			}
		}
	}

	if (chn_cnt > 1) {

		/* multiple channels - create a submenu, with 1 item per channel */

		Menu* chn_menu = manage (new Menu);
		MenuList& chn_items (chn_menu->items());
		Evoral::Parameter param_without_channel (auto_type, 0, cmd);

		/* add a couple of items to hide/show all of them */

		chn_items.push_back (
			MenuElem (_("Hide all channels"),
			          sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::change_all_channel_tracks_visibility),
			                      false, param_without_channel)));
		chn_items.push_back (
			MenuElem (_("Show all channels"),
			          sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::change_all_channel_tracks_visibility),
			                      true, param_without_channel)));

		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {

				/* for each selected channel, add a menu item for this controller */

				Evoral::Parameter fully_qualified_param (auto_type, chn, cmd);
				chn_items.push_back (
					CheckMenuElem (string_compose (_("Channel %1"), chn+1),
					               sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
					                           fully_qualified_param)));

				boost::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);
				bool visible = false;

				if (track) {
					if (track->marked_for_display()) {
						visible = true;
					}
				}

				Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*>(&chn_items.back());
				_channel_command_menu_map[fully_qualified_param] = cmi;
				cmi->set_active (visible);
			}
		}

		/* now create an item in the parent menu that has the per-channel list as a submenu */

		items.push_back (MenuElem (label, *chn_menu));

	} else {

		/* just one channel - create a single menu item for this command+channel combination*/

		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {

				Evoral::Parameter fully_qualified_param (auto_type, chn, cmd);
				items.push_back (
					CheckMenuElem (label,
					               sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
					                           fully_qualified_param)));

				boost::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);
				bool visible = false;

				if (track) {
					if (track->marked_for_display()) {
						visible = true;
					}
				}

				Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*>(&items.back());
				_channel_command_menu_map[fully_qualified_param] = cmi;
				cmi->set_active (visible);

				/* one channel only */
				break;
			}
		}
	}
}

/** Add a single menu item for a controller on one channel. */
void
MidiTimeAxisView::add_single_channel_controller_item(Menu_Helpers::MenuList& ctl_items,
                                                     int                     ctl,
                                                     const std::string&      name)
{
	using namespace Menu_Helpers;

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();
	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			ctl_items.push_back (
				CheckMenuElem (
					string_compose ("<b>%1</b>: %2 [%3]", ctl, name, int (chn + 1)),
					sigc::bind (
						sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
						fully_qualified_param)));
			dynamic_cast<Label*> (ctl_items.back().get_child())->set_use_markup (true);

			boost::shared_ptr<AutomationTimeAxisView> track = automation_child (
				fully_qualified_param);

			bool visible = false;
			if (track) {
				if (track->marked_for_display()) {
					visible = true;
				}
			}

			Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*>(&ctl_items.back());
			_controller_menu_map[fully_qualified_param] = cmi;
			cmi->set_active (visible);

			/* one channel only */
			break;
		}
	}
}

/** Add a submenu with 1 item per channel for a controller on many channels. */
void
MidiTimeAxisView::add_multi_channel_controller_item(Menu_Helpers::MenuList& ctl_items,
                                                    int                     ctl,
                                                    const std::string&      name)
{
	using namespace Menu_Helpers;

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();

	Menu* chn_menu = manage (new Menu);
	MenuList& chn_items (chn_menu->items());

	/* add a couple of items to hide/show this controller on all channels */

	Evoral::Parameter param_without_channel (MidiCCAutomation, 0, ctl);
	chn_items.push_back (
		MenuElem (_("Hide all channels"),
		          sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::change_all_channel_tracks_visibility),
		                      false, param_without_channel)));
	chn_items.push_back (
		MenuElem (_("Show all channels"),
		          sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::change_all_channel_tracks_visibility),
		                      true, param_without_channel)));

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {

			/* for each selected channel, add a menu item for this controller */

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			chn_items.push_back (
				CheckMenuElem (string_compose (_("Channel %1"), chn+1),
				               sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
				                           fully_qualified_param)));

			boost::shared_ptr<AutomationTimeAxisView> track = automation_child (
				fully_qualified_param);
			bool visible = false;

			if (track) {
				if (track->marked_for_display()) {
					visible = true;
				}
			}

			Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*>(&chn_items.back());
			_controller_menu_map[fully_qualified_param] = cmi;
			cmi->set_active (visible);
		}
	}

	/* add the per-channel menu to the list of controllers, with the name of the controller */
	ctl_items.push_back (MenuElem (string_compose ("<b>%1</b>: %2", ctl, name),
	                               *chn_menu));
	dynamic_cast<Label*> (ctl_items.back().get_child())->set_use_markup (true);
}

boost::shared_ptr<MIDI::Name::CustomDeviceMode>
MidiTimeAxisView::get_device_mode()
{
	using namespace MIDI::Name;

	boost::shared_ptr<MasterDeviceNames> device_names = get_device_names();
	if (!device_names) {
		return boost::shared_ptr<MIDI::Name::CustomDeviceMode>();
	}

	return device_names->custom_device_mode_by_name(
		gui_property (X_("midnam-custom-device-mode")));
}

boost::shared_ptr<MIDI::Name::MasterDeviceNames>
MidiTimeAxisView::get_device_names()
{
	using namespace MIDI::Name;

	const std::string model = gui_property (X_("midnam-model-name"));

	boost::shared_ptr<MIDINameDocument> midnam = MidiPatchManager::instance()
		.document_by_model(model);
	if (midnam) {
		return midnam->master_device_names(model);
	} else {
		return boost::shared_ptr<MasterDeviceNames>();
	}
}

void
MidiTimeAxisView::build_controller_menu ()
{
	using namespace Menu_Helpers;

	if (controller_menu) {
		/* it exists and has not been invalidated by a channel mode change */
		return;
	}

	controller_menu = new Menu; // explicitly managed by us
	MenuList& items (controller_menu->items());

	/* create several "top level" menu items for sets of controllers (16 at a
	   time), and populate each one with a submenu for each controller+channel
	   combination covering the currently selected channels for this track
	*/

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();

	/* count the number of selected channels because we will build a different menu
	   structure if there is more than 1 selected.
	*/

	int chn_cnt = 0;
	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {
			if (++chn_cnt > 1) {
				break;
			}
		}
	}

	using namespace MIDI::Name;
	boost::shared_ptr<MasterDeviceNames> device_names = get_device_names();

	if (device_names && !device_names->controls().empty()) {
		/* Controllers names available in midnam file, generate fancy menu */
		unsigned n_items  = 0;
		unsigned n_groups = 0;

		/* TODO: This is not correct, should look up the currently applicable ControlNameList
		   and only build a menu for that one. */
		for (MasterDeviceNames::ControlNameLists::const_iterator l = device_names->controls().begin();
		     l != device_names->controls().end(); ++l) {
			boost::shared_ptr<ControlNameList> name_list = l->second;
			Menu*                              ctl_menu  = NULL;

			for (ControlNameList::Controls::const_iterator c = name_list->controls().begin();
			     c != name_list->controls().end();) {
				const uint16_t ctl = c->second->number();
				if (ctl != MIDI_CTL_MSB_BANK && ctl != MIDI_CTL_LSB_BANK) {
					/* Skip bank select controllers since they're handled specially */
					if (n_items == 0) {
						/* Create a new submenu */
						ctl_menu = manage (new Menu);
					}

					MenuList& ctl_items (ctl_menu->items());
					if (chn_cnt > 1) {
						add_multi_channel_controller_item(ctl_items, ctl, c->second->name());
					} else {
						add_single_channel_controller_item(ctl_items, ctl, c->second->name());
					}
				}

				++c;
				if (ctl_menu && (++n_items == 16 || c == name_list->controls().end())) {
					/* Submenu has 16 items or we're done, add it to controller menu and reset */
					items.push_back(
						MenuElem(string_compose(_("Controllers %1-%2"),
						                        (16 * n_groups), (16 * n_groups) + n_items - 1),
						         *ctl_menu));
					ctl_menu = NULL;
					n_items  = 0;
					++n_groups;
				}
			}
		}
	} else {
		/* No controllers names, generate generic numeric menu */
		for (int i = 0; i < 127; i += 16) {
			Menu*     ctl_menu = manage (new Menu);
			MenuList& ctl_items (ctl_menu->items());

			for (int ctl = i; ctl < i+16; ++ctl) {
				if (ctl == MIDI_CTL_MSB_BANK || ctl == MIDI_CTL_LSB_BANK) {
					/* Skip bank select controllers since they're handled specially */
					continue;
				}

				if (chn_cnt > 1) {
					add_multi_channel_controller_item(
						ctl_items, ctl, string_compose(_("Controller %1"), ctl));
				} else {
					add_single_channel_controller_item(
						ctl_items, ctl, string_compose(_("Controller %1"), ctl));
				}
			}

			/* Add submenu for this block of controllers to controller menu */
			items.push_back (
				MenuElem (string_compose (_("Controllers %1-%2"), i, i + 15),
				          *ctl_menu));
		}
	}
}

Gtk::Menu*
MidiTimeAxisView::build_note_mode_menu()
{
	using namespace Menu_Helpers;

	Menu* mode_menu = manage (new Menu);
	MenuList& items = mode_menu->items();
	mode_menu->set_name ("ArdourContextMenu");

	RadioMenuItem::Group mode_group;
	items.push_back (
		RadioMenuElem (mode_group,_("Sustained"),
		               sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_mode),
		                           Sustained, true)));
	_note_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_note_mode_item->set_active(_note_mode == Sustained);

	items.push_back (
		RadioMenuElem (mode_group, _("Percussive"),
		               sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_mode),
		                           Percussive, true)));
	_percussion_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_percussion_mode_item->set_active(_note_mode == Percussive);

	return mode_menu;
}

Gtk::Menu*
MidiTimeAxisView::build_color_mode_menu()
{
	using namespace Menu_Helpers;

	Menu* mode_menu = manage (new Menu);
	MenuList& items = mode_menu->items();
	mode_menu->set_name ("ArdourContextMenu");

	RadioMenuItem::Group mode_group;
	items.push_back (
		RadioMenuElem (mode_group, _("Meter Colors"),
		               sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode),
		                           MeterColors, false, true, true)));
	_meter_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_meter_color_mode_item->set_active(_color_mode == MeterColors);

	items.push_back (
		RadioMenuElem (mode_group, _("Channel Colors"),
		               sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode),
		                           ChannelColors, false, true, true)));
	_channel_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_channel_color_mode_item->set_active(_color_mode == ChannelColors);

	items.push_back (
		RadioMenuElem (mode_group, _("Track Color"),
		               sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode),
		                           TrackColor, false, true, true)));
	_channel_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_channel_color_mode_item->set_active(_color_mode == TrackColor);

	return mode_menu;
}

Gtk::Menu*
MidiTimeAxisView::build_patch_menu()
{
	using namespace MIDI::Name;
	using namespace Menu_Helpers;

	boost::shared_ptr<MasterDeviceNames> device_names = get_device_names();
	const std::string device_mode = gui_property (X_("midnam-custom-device-mode"));

	Menu* pc_menu = manage (new Menu);
	MenuList& pc_items = pc_menu->items();

	for (uint32_t chn = 0; chn < 16; ++chn) {
		boost::shared_ptr<ChannelNameSet> channel_name_set = device_names->channel_name_set_by_channel (device_mode, chn);
		// see also PatchChange::initialize_popup_menus
		if (!channel_name_set) {
			continue;
		}
		const ChannelNameSet::PatchBanks& patch_banks = channel_name_set->patch_banks();
		if (patch_banks.size () == 0) {
			continue;
		}

		Gtk::Menu& chan_menu = *manage(new Gtk::Menu());

		if (patch_banks.size() > 1) {

			for (ChannelNameSet::PatchBanks::const_iterator bank = patch_banks.begin();
					bank != patch_banks.end();
					++bank) {
				Glib::RefPtr<Glib::Regex> underscores = Glib::Regex::create("_");
				std::string replacement(" ");

				Gtk::Menu& patch_bank_menu = *manage(new Gtk::Menu());

				const PatchNameList& patches = (*bank)->patch_name_list();
				Gtk::Menu::MenuList& patch_menus = patch_bank_menu.items();

				for (PatchNameList::const_iterator patch = patches.begin();
						patch != patches.end();
						++patch) {
					std::string name = underscores->replace((*patch)->name().c_str(), -1, 0, replacement);

					patch_menus.push_back(
							Gtk::Menu_Helpers::MenuElem(
								name,
								sigc::bind(
									sigc::mem_fun(*this, &MidiTimeAxisView::on_patch_menu_selected),
									chn, (*patch)->patch_primary_key())) );
				}


				std::string name = underscores->replace((*bank)->name().c_str(), -1, 0, replacement);

				chan_menu.items().push_back(
						Gtk::Menu_Helpers::MenuElem(
							name,
							patch_bank_menu) );
			}
		} else {
			/* only one patch bank, so make it the initial menu */

			const PatchNameList& patches = patch_banks.front()->patch_name_list();

			for (PatchNameList::const_iterator patch = patches.begin();
					patch != patches.end();
					++patch) {
				std::string name = (*patch)->name();
				boost::replace_all (name, "_", " ");

				chan_menu.items().push_back (
						Gtk::Menu_Helpers::MenuElem (
							name,
							sigc::bind (sigc::mem_fun(*this, &MidiTimeAxisView::on_patch_menu_selected),
								chn, (*patch)->patch_primary_key())));
			}
		}

		pc_items.push_back(
				Gtk::Menu_Helpers::MenuElem(
					string_compose (_("Channel %1"), chn + 1),
					chan_menu));
	}
	return pc_menu;
}

void
MidiTimeAxisView::on_patch_menu_selected (int chn, const MIDI::Name::PatchPrimaryKey& key)
{
	if (!_route) {
		return;
	}
	boost::shared_ptr<AutomationControl> bank_msb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_MSB_BANK), true);
	boost::shared_ptr<AutomationControl> bank_lsb = _route->automation_control(Evoral::Parameter (MidiCCAutomation, chn, MIDI_CTL_LSB_BANK), true);
	boost::shared_ptr<AutomationControl> program = _route->automation_control(Evoral::Parameter (MidiPgmChangeAutomation, chn), true);

	if (!bank_msb || ! bank_lsb || !program) {
		return;
	}
	bank_msb->set_value ((key.bank() >> 7) & 0x7f, Controllable::NoGroup);
	bank_lsb->set_value (key.bank() & 0x7f, Controllable::NoGroup);
	program->set_value (key.program(), Controllable::NoGroup);
}

void
MidiTimeAxisView::set_note_mode(NoteMode mode, bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (
			boost::bind (&MidiTimeAxisView::set_note_mode, _1, mode, false));
	} else {
		if (_note_mode != mode || midi_track()->note_mode() != mode) {
			_note_mode = mode;
			midi_track()->set_note_mode(mode);
			set_gui_property ("note-mode", enum_2_string(_note_mode));
			_view->redisplay_track();
		}
	}
}

void
MidiTimeAxisView::set_color_mode (ColorMode mode, bool force, bool redisplay, bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (
			boost::bind (&MidiTimeAxisView::set_color_mode, _1, mode, force, redisplay, false));
	} else {
		if (_color_mode == mode && !force) {
			return;
		}

		if (_channel_selector) {
			if (mode == ChannelColors) {
				_channel_selector->set_channel_colors(NoteBase::midi_channel_colors);
			} else {
				_channel_selector->set_default_channel_color();
			}
		}

		_color_mode = mode;
		set_gui_property ("color-mode", enum_2_string(_color_mode));
		if (redisplay) {
			_view->redisplay_track();
		}
	}
}

void
MidiTimeAxisView::set_note_range (MidiStreamView::VisibleNoteRange range, bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (
			boost::bind (&MidiTimeAxisView::set_note_range, _1, range, false));
	} else {
		if (!_ignore_signals) {
			midi_view()->set_note_range(range);
		}
	}
}

void
MidiTimeAxisView::update_range()
{
}

void
MidiTimeAxisView::show_all_automation (bool apply_to_selection)
{
	using namespace MIDI::Name;

	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (
			boost::bind (&MidiTimeAxisView::show_all_automation, _1, false));
	} else {
		if (midi_track()) {
			// Show existing automation
			const set<Evoral::Parameter> params = midi_track()->midi_playlist()->contained_automation();

			for (set<Evoral::Parameter>::const_iterator i = params.begin(); i != params.end(); ++i) {
				create_automation_child(*i, true);
			}

			// Show automation for all controllers named in midnam file
			boost::shared_ptr<MasterDeviceNames> device_names = get_device_names();
			if (gui_property (X_("midnam-model-name")) != "Generic" &&
			     device_names && !device_names->controls().empty()) {
				const std::string device_mode       = gui_property (X_("midnam-custom-device-mode"));
				const uint16_t    selected_channels = midi_track()->get_playback_channel_mask();
				for (uint32_t chn = 0; chn < 16; ++chn) {
					if ((selected_channels & (0x0001 << chn)) == 0) {
						// Channel not in use
						continue;
					}

					boost::shared_ptr<ChannelNameSet> chan_names = device_names->channel_name_set_by_channel(
						device_mode, chn);
					if (!chan_names) {
						continue;
					}

					boost::shared_ptr<ControlNameList> control_names = device_names->control_name_list(
						chan_names->control_list_name());
					if (!control_names) {
						continue;
					}

					for (ControlNameList::Controls::const_iterator c = control_names->controls().begin();
					     c != control_names->controls().end();
					     ++c) {
						const uint16_t ctl = c->second->number();
						if (ctl != MIDI_CTL_MSB_BANK && ctl != MIDI_CTL_LSB_BANK) {
							/* Skip bank select controllers since they're handled specially */
							const Evoral::Parameter param(MidiCCAutomation, chn, ctl);
							create_automation_child(param, true);
						}
					}
				}
			}
		}

		RouteTimeAxisView::show_all_automation ();
	}
}

void
MidiTimeAxisView::show_existing_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (
			boost::bind (&MidiTimeAxisView::show_existing_automation, _1, false));
	} else {
		if (midi_track()) {
			const set<Evoral::Parameter> params = midi_track()->midi_playlist()->contained_automation();

			for (set<Evoral::Parameter>::const_iterator i = params.begin(); i != params.end(); ++i) {
				create_automation_child (*i, true);
			}
		}

		RouteTimeAxisView::show_existing_automation ();
	}
}

/** Create an automation track for the given parameter (pitch bend, channel pressure).
 */
void
MidiTimeAxisView::create_automation_child (const Evoral::Parameter& param, bool show)
{
	if (param.type() == NullAutomation) {
		return;
	}

	AutomationTracks::iterator existing = _automation_tracks.find (param);

	if (existing != _automation_tracks.end()) {

		/* automation track created because we had existing data for
		 * the processor, but visibility may need to be controlled
		 * since it will have been set visible by default.
		 */

		existing->second->set_marked_for_display (show);

		if (!no_redraw) {
			request_redraw ();
		}

		return;
	}

	boost::shared_ptr<AutomationTimeAxisView> track;
	boost::shared_ptr<AutomationControl> control;


	switch (param.type()) {

	case GainAutomation:
		create_gain_automation_child (param, show);
		break;

	case MuteAutomation:
		create_mute_automation_child (param, show);
		break;

	case PluginAutomation:
		/* handled elsewhere */
		break;

	case MidiCCAutomation:
	case MidiPgmChangeAutomation:
	case MidiPitchBenderAutomation:
	case MidiChannelPressureAutomation:
	case MidiSystemExclusiveAutomation:
		/* These controllers are region "automation" - they are owned
		 * by regions (and their MidiModels), not by the track. As a
		 * result there is no AutomationList/Line for the track, but we create
		 * a controller for the user to write immediate events, so the editor
		 * can act as a control surface for the present MIDI controllers.
		 *
		 * TODO: Record manipulation of the controller to regions?
		 */

		control = _route->automation_control(param, true);
		track.reset (new AutomationTimeAxisView (
			             _session,
			             _route,
			             control ? _route : boost::shared_ptr<Automatable> (),
			             control,
			             param,
			             _editor,
			             *this,
			             true,
			             parent_canvas,
			             _route->describe_parameter(param)));

		if (_view) {
			_view->foreach_regionview (
				sigc::mem_fun (*track.get(), &TimeAxisView::add_ghost));
		}

		add_automation_child (param, track, show);
		break;

	case PanWidthAutomation:
	case PanElevationAutomation:
	case PanAzimuthAutomation:
		ensure_pan_views (show);
		break;

	default:
		error << "MidiTimeAxisView: unknown automation child "
		      << EventTypeMap::instance().to_symbol(param) << endmsg;
	}
}

void
MidiTimeAxisView::route_active_changed ()
{
	RouteUI::route_active_changed ();
	update_control_names();
}

void
MidiTimeAxisView::update_control_names ()
{
	if (is_track()) {
		if (_route->active()) {
			controls_base_selected_name = "MidiTrackControlsBaseSelected";
			controls_base_unselected_name = "MidiTrackControlsBaseUnselected";
		} else {
			controls_base_selected_name = "MidiTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "MidiTrackControlsBaseInactiveUnselected";
		}
	} else { // MIDI bus (which doesn't exist yet..)
		if (_route->active()) {
			controls_base_selected_name = "BusControlsBaseSelected";
			controls_base_unselected_name = "BusControlsBaseUnselected";
		} else {
			controls_base_selected_name = "BusControlsBaseInactiveSelected";
			controls_base_unselected_name = "BusControlsBaseInactiveUnselected";
		}
	}

	if (selected()) {
		controls_ebox.set_name (controls_base_selected_name);
		time_axis_frame.set_name (controls_base_selected_name);
	} else {
		controls_ebox.set_name (controls_base_unselected_name);
		time_axis_frame.set_name (controls_base_unselected_name);
	}
}

void
MidiTimeAxisView::set_note_selection (uint8_t note)
{
	uint16_t chn_mask = midi_track()->get_playback_channel_mask();

	_editor.begin_reversible_selection_op (X_("Set Note Selection"));

	if (_view->num_selected_regionviews() == 0) {
		_view->foreach_regionview (
			sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_selection_region_view),
			            note, chn_mask));
	} else {
		_view->foreach_selected_regionview (
			sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_selection_region_view),
			            note, chn_mask));
	}

	_editor.commit_reversible_selection_op();
}

void
MidiTimeAxisView::add_note_selection (uint8_t note)
{
	const uint16_t chn_mask = midi_track()->get_playback_channel_mask();

	_editor.begin_reversible_selection_op (X_("Add Note Selection"));

	if (_view->num_selected_regionviews() == 0) {
		_view->foreach_regionview (
			sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::add_note_selection_region_view),
			            note, chn_mask));
	} else {
		_view->foreach_selected_regionview (
			sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::add_note_selection_region_view),
			            note, chn_mask));
	}

	_editor.commit_reversible_selection_op();
}

void
MidiTimeAxisView::extend_note_selection (uint8_t note)
{
	const uint16_t chn_mask = midi_track()->get_playback_channel_mask();

	_editor.begin_reversible_selection_op (X_("Extend Note Selection"));

	if (_view->num_selected_regionviews() == 0) {
		_view->foreach_regionview (
			sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::extend_note_selection_region_view),
			            note, chn_mask));
	} else {
		_view->foreach_selected_regionview (
			sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::extend_note_selection_region_view),
			            note, chn_mask));
	}

	_editor.commit_reversible_selection_op();
}

void
MidiTimeAxisView::toggle_note_selection (uint8_t note)
{
	const uint16_t chn_mask = midi_track()->get_playback_channel_mask();

	_editor.begin_reversible_selection_op (X_("Toggle Note Selection"));

	if (_view->num_selected_regionviews() == 0) {
		_view->foreach_regionview (
			sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::toggle_note_selection_region_view),
			            note, chn_mask));
	} else {
		_view->foreach_selected_regionview (
			sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::toggle_note_selection_region_view),
			            note, chn_mask));
	}

	_editor.commit_reversible_selection_op();
}

void
MidiTimeAxisView::get_per_region_note_selection (list<pair<PBD::ID, set<boost::shared_ptr<Evoral::Note<Evoral::Beats> > > > >& selection)
{
	_view->foreach_regionview (
		sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::get_per_region_note_selection_region_view), sigc::ref(selection)));
}

void
MidiTimeAxisView::set_note_selection_region_view (RegionView* rv, uint8_t note, uint16_t chn_mask)
{
	dynamic_cast<MidiRegionView*>(rv)->select_matching_notes (note, chn_mask, false, false);
}

void
MidiTimeAxisView::add_note_selection_region_view (RegionView* rv, uint8_t note, uint16_t chn_mask)
{
	dynamic_cast<MidiRegionView*>(rv)->select_matching_notes (note, chn_mask, true, false);
}

void
MidiTimeAxisView::extend_note_selection_region_view (RegionView* rv, uint8_t note, uint16_t chn_mask)
{
	dynamic_cast<MidiRegionView*>(rv)->select_matching_notes (note, chn_mask, true, true);
}

void
MidiTimeAxisView::toggle_note_selection_region_view (RegionView* rv, uint8_t note, uint16_t chn_mask)
{
	dynamic_cast<MidiRegionView*>(rv)->toggle_matching_notes (note, chn_mask);
}

void
MidiTimeAxisView::get_per_region_note_selection_region_view (RegionView* rv, list<pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Evoral::Beats> > > > > &selection)
{
	Evoral::Sequence<Evoral::Beats>::Notes selected;
	dynamic_cast<MidiRegionView*>(rv)->selection_as_notelist (selected, false);

	std::set<boost::shared_ptr<Evoral::Note<Evoral::Beats> > > notes;

	Evoral::Sequence<Evoral::Beats>::Notes::iterator sel_it;
	for (sel_it = selected.begin(); sel_it != selected.end(); ++sel_it) {
		notes.insert (*sel_it);
	}

	if (!notes.empty()) {
		selection.push_back (make_pair ((rv)->region()->id(), notes));
	}
}

void
MidiTimeAxisView::set_channel_mode (ChannelMode, uint16_t)
{
	/* hide all automation tracks that use the wrong channel(s) and show all those that use
	   the right ones.
	*/

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();
	bool changed = false;

	no_redraw = true;

	for (uint32_t ctl = 0; ctl < 127; ++ctl) {

		for (uint32_t chn = 0; chn < 16; ++chn) {
			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			boost::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);

			if (!track) {
				continue;
			}

			if ((selected_channels & (0x0001 << chn)) == 0) {
				/* channel not in use. hiding it will trigger RouteTimeAxisView::automation_track_hidden()
				   which will cause a redraw. We don't want one per channel, so block that with no_redraw.
				*/
				changed = track->set_marked_for_display (false) || changed;
			} else {
				changed = track->set_marked_for_display (true) || changed;
			}
		}
	}

	no_redraw = false;

	/* TODO: Bender, Pressure */

	/* invalidate the controller menu, so that we rebuild it next time */
	_controller_menu_map.clear ();
	delete controller_menu;
	controller_menu = 0;

	if (changed) {
		request_redraw ();
	}
}

Gtk::CheckMenuItem*
MidiTimeAxisView::automation_child_menu_item (Evoral::Parameter param)
{
	Gtk::CheckMenuItem* m = RouteTimeAxisView::automation_child_menu_item (param);
	if (m) {
		return m;
	}

	ParameterMenuMap::iterator i = _controller_menu_map.find (param);
	if (i != _controller_menu_map.end()) {
		return i->second;
	}

	i = _channel_command_menu_map.find (param);
	if (i != _channel_command_menu_map.end()) {
		return i->second;
	}

	return 0;
}

boost::shared_ptr<MidiRegion>
MidiTimeAxisView::add_region (framepos_t f, framecnt_t length, bool commit)
{
	Editor* real_editor = dynamic_cast<Editor*> (&_editor);
	MusicFrame pos (f, 0);

	if (commit) {
		real_editor->begin_reversible_command (Operations::create_region);
	}
	playlist()->clear_changes ();

	real_editor->snap_to (pos, RoundNearest);

	boost::shared_ptr<Source> src = _session->create_midi_source_by_stealing_name (view()->trackview().track());
	PropertyList plist;

	plist.add (ARDOUR::Properties::start, 0);
	plist.add (ARDOUR::Properties::length, length);
	plist.add (ARDOUR::Properties::name, PBD::basename_nosuffix(src->name()));

	boost::shared_ptr<Region> region = (RegionFactory::create (src, plist));
	/* sets beat position */
	region->set_position (pos.frame, pos.division);
	playlist()->add_region (region, pos.frame, 1.0, false, pos.division);
	_session->add_command (new StatefulDiffCommand (playlist()));

	if (commit) {
		real_editor->commit_reversible_command ();
	}

	return boost::dynamic_pointer_cast<MidiRegion>(region);
}

void
MidiTimeAxisView::ensure_step_editor ()
{
	if (!_step_editor) {
		_step_editor = new StepEditor (_editor, midi_track(), *this);
	}
}

void
MidiTimeAxisView::start_step_editing ()
{
	ensure_step_editor ();
	_step_editor->start_step_editing ();

}
void
MidiTimeAxisView::stop_step_editing ()
{
	if (_step_editor) {
		_step_editor->stop_step_editing ();
	}
}

/** @return channel (counted from 0) to add an event to, based on the current setting
 *  of the channel selector.
 */
uint8_t
MidiTimeAxisView::get_channel_for_add () const
{
	uint16_t const chn_mask = midi_track()->get_playback_channel_mask();
	int chn_cnt = 0;
	uint8_t channel = 0;

	/* pick the highest selected channel, unless all channels are selected,
	   which is interpreted to mean channel 1 (zero)
	*/

	for (uint16_t i = 0; i < 16; ++i) {
		if (chn_mask & (1<<i)) {
			channel = i;
			chn_cnt++;
		}
	}

	if (chn_cnt == 16) {
		channel = 0;
	}

	return channel;
}

void
MidiTimeAxisView::note_range_changed ()
{
	set_gui_property ("note-range-min", (int) midi_view()->lowest_note ());
	set_gui_property ("note-range-max", (int) midi_view()->highest_note ());
}

void
MidiTimeAxisView::contents_height_changed ()
{
	_range_scroomer->queue_resize ();
}

bool
MidiTimeAxisView::paste (framepos_t pos, const Selection& selection, PasteContext& ctx, const int32_t sub_num)
{
	if (!_editor.internal_editing()) {
		// Non-internal paste, paste regions like any other route
		return RouteTimeAxisView::paste(pos, selection, ctx, sub_num);
	}

	return midi_view()->paste(pos, selection, ctx, sub_num);
}
