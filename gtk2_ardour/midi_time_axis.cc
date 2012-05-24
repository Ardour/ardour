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
#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_object.h"
#include "ardour/source.h"
#include "ardour/track.h"
#include "ardour/types.h"

#include "midi++/names.h"

#include "ardour_ui.h"
#include "ardour_button.h"
#include "automation_line.h"
#include "automation_time_axis.h"
#include "canvas-note-event.h"
#include "canvas_impl.h"
#include "editor.h"
#include "enums.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "keyboard.h"
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
#include "simplerect.h"
#include "utils.h"

#include "ardour/midi_track.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;

// Minimum height at which a control is displayed
static const uint32_t MIDI_CONTROLS_BOX_MIN_HEIGHT = 162;
static const uint32_t KEYBOARD_MIN_HEIGHT = 140;

MidiTimeAxisView::MidiTimeAxisView (PublicEditor& ed, Session* sess, Canvas& canvas)
	: AxisView(sess) // virtually inherited
	, RouteTimeAxisView(ed, sess, canvas)
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
	, _step_edit_item (0)
	, _midi_thru_item (0)
	, controller_menu (0)
        , _step_editor (0)
{
}

void
MidiTimeAxisView::set_route (boost::shared_ptr<Route> rt)
{
	_route = rt;
	
	_view = new MidiStreamView (*this);
	
	if (is_track ()) {
		_piano_roll_header = new PianoRollHeader(*midi_view());
		_range_scroomer = new MidiScroomer(midi_view()->note_range_adjustment);
	}

	/* This next call will result in our height being set up, so it must come after
	   the creation of the piano roll / range scroomer as their visibility is set up
	   when our height is.
	*/
	RouteTimeAxisView::set_route (rt);

	_view->apply_color (_color, StreamView::RegionColor);

	subplugin_menu.set_name ("ArdourContextMenu");

	if (!gui_property ("note-range-min").empty ()) {
		midi_view()->apply_note_range (atoi (gui_property ("note-range-min").c_str()), atoi (gui_property ("note-range-max").c_str()), true);
	}
	midi_view()->NoteRangeChanged.connect (sigc::mem_fun (*this, &MidiTimeAxisView::note_range_changed));

	ignore_toggle = false;

	if (is_midi_track()) {
		controls_ebox.set_name ("MidiTimeAxisViewControlsBaseUnselected");
		_note_mode = midi_track()->note_mode();
	} else { // MIDI bus (which doesn't exist yet..)
		controls_ebox.set_name ("MidiBusControlsBaseUnselected");
	}

	/* map current state of the route */

	processors_changed (RouteProcessorChange ());

	_route->processors_changed.connect (*this, invalidator (*this), boost::bind (&MidiTimeAxisView::processors_changed, this, _1), gui_context());

	if (is_track()) {
		_piano_roll_header->SetNoteSelection.connect (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_selection));
		_piano_roll_header->AddNoteSelection.connect (sigc::mem_fun (*this, &MidiTimeAxisView::add_note_selection));
		_piano_roll_header->ExtendNoteSelection.connect (sigc::mem_fun (*this, &MidiTimeAxisView::extend_note_selection));
		_piano_roll_header->ToggleNoteSelection.connect (sigc::mem_fun (*this, &MidiTimeAxisView::toggle_note_selection));

		/* Suspend updates of the StreamView during scroomer drags to speed things up */
		_range_scroomer->DragStarting.connect (sigc::mem_fun (*midi_view(), &MidiStreamView::suspend_updates));
		_range_scroomer->DragFinishing.connect (sigc::mem_fun (*midi_view(), &MidiStreamView::resume_updates));

		controls_hbox.pack_start(*_range_scroomer);
		controls_hbox.pack_start(*_piano_roll_header);

		controls_ebox.set_name ("MidiTrackControlsBaseUnselected");
		controls_base_selected_name = "MidiTrackControlsBaseSelected";
		controls_base_unselected_name = "MidiTrackControlsBaseUnselected";

		midi_view()->NoteRangeChanged.connect (sigc::mem_fun(*this, &MidiTimeAxisView::update_range));

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (sigc::mem_fun(*this, &MidiTimeAxisView::region_view_added));

		if (!_editor.have_idled()) {
			/* first idle will do what we need */
		} else {
			first_idle ();
		}
	}

	HBox* midi_controls_hbox = manage(new HBox());

	MIDI::Name::MidiPatchManager& patch_manager = MIDI::Name::MidiPatchManager::instance();

	MIDI::Name::MasterDeviceNames::Models::const_iterator m = patch_manager.all_models().begin();
	for (; m != patch_manager.all_models().end(); ++m) {
		_model_selector.append_text(m->c_str());
	}

	_model_selector.signal_changed().connect(sigc::mem_fun(*this, &MidiTimeAxisView::model_changed));

	_custom_device_mode_selector.signal_changed().connect(
			sigc::mem_fun(*this, &MidiTimeAxisView::custom_device_mode_changed));

	// TODO: persist the choice
	// this initializes the comboboxes and sends out the signal
	_model_selector.set_active(0);

	midi_controls_hbox->pack_start(_channel_selector, true, false);
	if (!patch_manager.all_models().empty()) {
		_midi_controls_box.pack_start(_model_selector, true, false);
		_midi_controls_box.pack_start(_custom_device_mode_selector, true, false);
	}

	_midi_controls_box.pack_start(*midi_controls_hbox, true, true);

	controls_vbox.pack_start(_midi_controls_box, false, false);

	// restore channel selector settings
	_channel_selector.set_channel_mode(midi_track()->get_channel_mode(), midi_track()->get_channel_mask());
	_channel_selector.mode_changed.connect(
		sigc::mem_fun(*midi_track(), &MidiTrack::set_channel_mode));
	_channel_selector.mode_changed.connect(
		sigc::mem_fun(*this, &MidiTimeAxisView::set_channel_mode));

	string prop = gui_property ("color-mode");
	if (!prop.empty()) {
		_color_mode = ColorMode (string_2_enum(prop, _color_mode));
		if (_color_mode == ChannelColors) {
			_channel_selector.set_channel_colors(CanvasNoteEvent::midi_channel_colors);
		}
	}

	set_color_mode (_color_mode, true, false);

	prop = gui_property ("note-mode");
	if (!prop.empty()) {
		_note_mode = NoteMode (string_2_enum (prop, _note_mode));
		if (_percussion_mode_item) {
			_percussion_mode_item->set_active (_note_mode == Percussive);
		}
	}

	/* Look for any GUI object state nodes that represent automation children that should exist, and create
	 * the children.
	 */

	list<string> gui_ids = gui_object_state().all_ids ();
	for (list<string>::const_iterator i = gui_ids.begin(); i != gui_ids.end(); ++i) {
		PBD::ID route_id;
		bool has_parameter;
		Evoral::Parameter parameter (0, 0, 0);

		bool const p = AutomationTimeAxisView::parse_state_id (*i, route_id, has_parameter, parameter);
		if (p && route_id == _route->id () && has_parameter) {
			create_automation_child (parameter, string_is_affirmative (gui_object_state().get_string (*i, X_("visible"))));
		}
	}
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
	delete _piano_roll_header;
	_piano_roll_header = 0;

	delete _range_scroomer;
	_range_scroomer = 0;

	delete controller_menu;
        delete _step_editor;
}

void
MidiTimeAxisView::enter_internal_edit_mode ()
{
        if (midi_view()) {
                midi_view()->enter_internal_edit_mode ();
        }
}

void
MidiTimeAxisView::leave_internal_edit_mode ()
{
        if (midi_view()) {
                midi_view()->leave_internal_edit_mode ();
        }
}

void
MidiTimeAxisView::check_step_edit ()
{
	ensure_step_editor ();
        _step_editor->check_step_edit ();
}

void
MidiTimeAxisView::model_changed()
{
	std::list<std::string> device_modes = MIDI::Name::MidiPatchManager::instance()
		.custom_device_mode_names_by_model(_model_selector.get_active_text());

	_custom_device_mode_selector.clear_items();

	for (std::list<std::string>::const_iterator i = device_modes.begin();
			i != device_modes.end(); ++i) {
		_custom_device_mode_selector.append_text(*i);
	}

	_custom_device_mode_selector.set_active(0);
}

void MidiTimeAxisView::custom_device_mode_changed()
{
	_midi_patch_settings_changed.emit(_model_selector.get_active_text(),
			_custom_device_mode_selector.get_active_text());
}

MidiStreamView*
MidiTimeAxisView::midi_view()
{
	return dynamic_cast<MidiStreamView*>(_view);
}

void
MidiTimeAxisView::set_height (uint32_t h)
{
	if (h >= MIDI_CONTROLS_BOX_MIN_HEIGHT) {
		_midi_controls_box.show_all ();
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
	RouteTimeAxisView::set_height (h);
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

	range_items.push_back (MenuElem (_("Show Full Range"), sigc::bind (
			sigc::mem_fun(*this, &MidiTimeAxisView::set_note_range),
			MidiStreamView::FullRange)));

	range_items.push_back (MenuElem (_("Fit Contents"), sigc::bind (
			sigc::mem_fun(*this, &MidiTimeAxisView::set_note_range),
			MidiStreamView::ContentsRange)));

	items.push_back (MenuElem (_("Note Range"), *range_menu));
	items.push_back (MenuElem (_("Note Mode"), *build_note_mode_menu()));

	items.push_back (CheckMenuElem (_("MIDI Thru"), sigc::mem_fun(*this, &MidiTimeAxisView::toggle_midi_thru)));
	_midi_thru_item = dynamic_cast<CheckMenuItem*>(&items.back());

	items.push_back (SeparatorElem ());
}

void
MidiTimeAxisView::toggle_midi_thru ()
{
	if (!_midi_thru_item) {
		return;
	}

	bool view_yn = _midi_thru_item->get_active();
	if (view_yn != midi_track()->midi_thru()) {
		midi_track()->set_midi_thru (view_yn);
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

	uint16_t selected_channels = _channel_selector.get_selected_channels();

	if (selected_channels !=  0) {

		automation_items.push_back (SeparatorElem());

		/* these 2 MIDI "command" types are semantically more like automation than note data,
		   but they are not MIDI controllers. We give them special status in this menu, since
		   they will not show up in the controller list and anyone who actually knows
		   something about MIDI (!) would not expect to find them there.
		*/

		add_channel_command_menu_item (automation_items, _("Bender"), MidiPitchBenderAutomation, 0);
		automation_items.back().set_sensitive (!for_selection || _editor.get_selection().tracks.size() == 1);
		add_channel_command_menu_item (automation_items, _("Pressure"), MidiChannelPressureAutomation, 0);
		automation_items.back().set_sensitive (!for_selection || _editor.get_selection().tracks.size() == 1);

		/* now all MIDI controllers. Always offer the possibility that we will rebuild the controllers menu
		   since it might need to be updated after a channel mode change or other change. Also detach it
		   first in case it has been used anywhere else.
		*/

		build_controller_menu ();

		automation_items.push_back (SeparatorElem());
		automation_items.push_back (MenuElem (_("Controllers"), *controller_menu));
		automation_items.back().set_sensitive (!for_selection || _editor.get_selection().tracks.size() == 1);
	} else {
		automation_items.push_back (MenuElem (string_compose ("<i>%1</i>", _("No MIDI Channels selected"))));
		dynamic_cast<Label*> (automation_items.back().get_child())->set_use_markup (true);
	}

}

void
MidiTimeAxisView::change_all_channel_tracks_visibility (bool yn, Evoral::Parameter param)
{
	uint16_t selected_channels = _channel_selector.get_selected_channels();

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
MidiTimeAxisView::add_channel_command_menu_item (Menu_Helpers::MenuList& items, const string& label, AutomationType auto_type, uint8_t cmd)
{
	using namespace Menu_Helpers;

	/* count the number of selected channels because we will build a different menu structure if there is more than 1 selected.
	 */

	uint16_t selected_channels = _channel_selector.get_selected_channels();
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

		chn_items.push_back (MenuElem (_("Hide all channels"),
						    sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::change_all_channel_tracks_visibility),
								false, param_without_channel)));
		chn_items.push_back (MenuElem (_("Show all channels"),
						    sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::change_all_channel_tracks_visibility),
								true, param_without_channel)));

		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {

				/* for each selected channel, add a menu item for this controller */

				Evoral::Parameter fully_qualified_param (auto_type, chn, cmd);
				chn_items.push_back (CheckMenuElem (string_compose (_("Channel %1"), chn+1),
								    sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
										fully_qualified_param)));

				boost::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);
				bool visible = false;

				if (track) {
					if (track->marked_for_display()) {
						visible = true;
					}
				}

				CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&chn_items.back());
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
				items.push_back (CheckMenuElem (label,
								sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
									    fully_qualified_param)));

				boost::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);
				bool visible = false;

				if (track) {
					if (track->marked_for_display()) {
						visible = true;
					}
				}

				CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&items.back());
				_channel_command_menu_map[fully_qualified_param] = cmi;
				cmi->set_active (visible);

				/* one channel only */
				break;
			}
		}
	}
}

void
MidiTimeAxisView::build_controller_menu ()
{
	using namespace Menu_Helpers;

	if (controller_menu) {
		/* it exists and has not been invalidated by a channel mode change, so just return it */
		return;
	}

	controller_menu = new Menu; // explicitly managed by us
	MenuList& items (controller_menu->items());

	/* create several "top level" menu items for sets of controllers (16 at a time), and populate each one with a submenu
	   for each controller+channel combination covering the currently selected channels for this track
	*/

	uint16_t selected_channels = _channel_selector.get_selected_channels();

	/* count the number of selected channels because we will build a different menu structure if there is more than 1 selected.
	 */

	int chn_cnt = 0;

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (selected_channels & (0x0001 << chn)) {
			if (++chn_cnt > 1) {
				break;
			}
		}
	}

	/* loop over all 127 MIDI controllers, in groups of 16; except don't offer
	   bank select controllers, as they are handled by the `patch' code */

	for (int i = 0; i < 127; i += 16) {

		Menu* ctl_menu = manage (new Menu);
		MenuList& ctl_items (ctl_menu->items());


		/* for each controller, consider whether to create a submenu or a single item */

		for (int ctl = i; ctl < i+16; ++ctl) {

			if (ctl == MIDI_CTL_MSB_BANK || ctl == MIDI_CTL_LSB_BANK) {
				continue;
			}

			if (chn_cnt > 1) {

				/* multiple channels - create a submenu, with 1 item per channel */

				Menu* chn_menu = manage (new Menu);
				MenuList& chn_items (chn_menu->items());

				/* add a couple of items to hide/show this controller on all channels */

				Evoral::Parameter param_without_channel (MidiCCAutomation, 0, ctl);
				chn_items.push_back (MenuElem (_("Hide all channels"),
								    sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::change_all_channel_tracks_visibility),
										false, param_without_channel)));
				chn_items.push_back (MenuElem (_("Show all channels"),
								    sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::change_all_channel_tracks_visibility),
										true, param_without_channel)));

				for (uint8_t chn = 0; chn < 16; chn++) {
					if (selected_channels & (0x0001 << chn)) {

						/* for each selected channel, add a menu item for this controller */

						Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
						chn_items.push_back (CheckMenuElem (string_compose (_("Channel %1"), chn+1),
										    sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
												fully_qualified_param)));

						boost::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);
						bool visible = false;

						if (track) {
							if (track->marked_for_display()) {
								visible = true;
							}
						}

						CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&chn_items.back());
						_controller_menu_map[fully_qualified_param] = cmi;
						cmi->set_active (visible);
					}
				}

				/* add the per-channel menu to the list of controllers, with the name of the controller */
				ctl_items.push_back (MenuElem (string_compose ("<b>%1</b>: %2", ctl, midi_name (ctl)), *chn_menu));
				dynamic_cast<Label*> (ctl_items.back().get_child())->set_use_markup (true);

			} else {

				/* just one channel - create a single menu item for this ctl+channel combination*/

				for (uint8_t chn = 0; chn < 16; chn++) {
					if (selected_channels & (0x0001 << chn)) {

						Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
						ctl_items.push_back (
							CheckMenuElem (
								string_compose ("<b>%1</b>: %2 [%3]", ctl, midi_name (ctl), int (chn)),
								sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
									    fully_qualified_param)
								)
							);
						dynamic_cast<Label*> (ctl_items.back().get_child())->set_use_markup (true);

						boost::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);
						bool visible = false;

						if (track) {
							if (track->marked_for_display()) {
								visible = true;
							}
						}

						CheckMenuItem* cmi = static_cast<CheckMenuItem*>(&ctl_items.back());
						_controller_menu_map[fully_qualified_param] = cmi;
						cmi->set_active (visible);

						/* one channel only */
						break;
					}
				}
			}
		}

		/* add the menu for this block of controllers to the overall controller menu */

		items.push_back (MenuElem (string_compose (_("Controllers %1-%2"), i, i+15), *ctl_menu));
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
	items.push_back (RadioMenuElem (mode_group, _("Sustained"),
				sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_mode), Sustained)));
	_note_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_note_mode_item->set_active(_note_mode == Sustained);

	items.push_back (RadioMenuElem (mode_group, _("Percussive"),
				sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_mode), Percussive)));
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
	items.push_back (RadioMenuElem (mode_group, _("Meter Colors"),
	                                sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode),
	                                            MeterColors, false, true)));
	_meter_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_meter_color_mode_item->set_active(_color_mode == MeterColors);

	items.push_back (RadioMenuElem (mode_group, _("Channel Colors"),
	                                sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode),
	                                            ChannelColors, false, true)));
	_channel_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_channel_color_mode_item->set_active(_color_mode == ChannelColors);

	items.push_back (RadioMenuElem (mode_group, _("Track Color"),
	                                sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_color_mode),
	                                            TrackColor, false, true)));
	_channel_color_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_channel_color_mode_item->set_active(_color_mode == TrackColor);

	return mode_menu;
}

void
MidiTimeAxisView::set_note_mode(NoteMode mode)
{
	if (_note_mode != mode || midi_track()->note_mode() != mode) {
		_note_mode = mode;
		midi_track()->set_note_mode(mode);
		set_gui_property ("note-mode", enum_2_string(_note_mode));
		_view->redisplay_track();
	}
}

void
MidiTimeAxisView::set_color_mode (ColorMode mode, bool force, bool redisplay)
{
	if (_color_mode == mode && !force) {
		return;
	}

	if (mode == ChannelColors) {
		_channel_selector.set_channel_colors(CanvasNoteEvent::midi_channel_colors);
	} else {
		_channel_selector.set_default_channel_color();
	}

	_color_mode = mode;
	set_gui_property ("color-mode", enum_2_string(_color_mode));
	if (redisplay) {
		_view->redisplay_track();
	}
}

void
MidiTimeAxisView::set_note_range(MidiStreamView::VisibleNoteRange range)
{
	if (!_ignore_signals)
		midi_view()->set_note_range(range);
}


void
MidiTimeAxisView::update_range()
{
	MidiGhostRegion* mgr;

	for(list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		if ((mgr = dynamic_cast<MidiGhostRegion*>(*i)) != 0) {
			mgr->update_range();
		}
	}
}

void
MidiTimeAxisView::show_all_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (boost::bind (&MidiTimeAxisView::show_all_automation, _1, false));
	} else {
		if (midi_track()) {
			const set<Evoral::Parameter> params = midi_track()->midi_playlist()->contained_automation();

			for (set<Evoral::Parameter>::const_iterator i = params.begin(); i != params.end(); ++i) {
				create_automation_child(*i, true);
			}
		}

		RouteTimeAxisView::show_all_automation ();
	}
}

void
MidiTimeAxisView::show_existing_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (boost::bind (&MidiTimeAxisView::show_existing_automation, _1, false));
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

		if (existing->second->set_marked_for_display (show) && !no_redraw) {
			request_redraw ();
		}

		return;
	}

	boost::shared_ptr<AutomationTimeAxisView> track;

	switch (param.type()) {

	case GainAutomation:
		create_gain_automation_child (param, show);
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
		 * result we do not create an AutomationList/Line for the track
		 * ... except here we are doing something!! XXX 
		 */

		track.reset (new AutomationTimeAxisView (
				     _session,
				     _route,
				     boost::shared_ptr<Automatable> (),
				     boost::shared_ptr<AutomationControl> (),
				     param,
				     _editor,
				     *this,
				     true,
				     parent_canvas,
				     _route->describe_parameter(param)
				     ));

		if (_view) {
			_view->foreach_regionview (sigc::mem_fun (*track.get(), &TimeAxisView::add_ghost));
		}

		add_automation_child (param, track, show);
		break;

	default:
		error << "MidiTimeAxisView: unknown automation child " << EventTypeMap::instance().to_symbol(param) << endmsg;
	}
}

void
MidiTimeAxisView::route_active_changed ()
{
	RouteUI::route_active_changed ();

	if (is_track()) {
		if (_route->active()) {
			controls_ebox.set_name ("MidiTrackControlsBaseUnselected");
			controls_base_selected_name = "MidiTrackControlsBaseSelected";
			controls_base_unselected_name = "MidiTrackControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("MidiTrackControlsBaseInactiveUnselected");
			controls_base_selected_name = "MidiTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "MidiTrackControlsBaseInactiveUnselected";
		}
	} else {

		throw; // wha?

		if (_route->active()) {
			controls_ebox.set_name ("BusControlsBaseUnselected");
			controls_base_selected_name = "BusControlsBaseSelected";
			controls_base_unselected_name = "BusControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("BusControlsBaseInactiveUnselected");
			controls_base_selected_name = "BusControlsBaseInactiveSelected";
			controls_base_unselected_name = "BusControlsBaseInactiveUnselected";
		}
	}
}

void
MidiTimeAxisView::set_note_selection (uint8_t note)
{
	if (!_editor.internal_editing()) {
		return;
	}

	uint16_t chn_mask = _channel_selector.get_selected_channels();

	if (_view->num_selected_regionviews() == 0) {
		_view->foreach_regionview (sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_selection_region_view), note, chn_mask));
	} else {
		_view->foreach_selected_regionview (sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::set_note_selection_region_view), note, chn_mask));
	}
}

void
MidiTimeAxisView::add_note_selection (uint8_t note)
{
	if (!_editor.internal_editing()) {
		return;
	}

	uint16_t chn_mask = _channel_selector.get_selected_channels();

	if (_view->num_selected_regionviews() == 0) {
		_view->foreach_regionview (sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::add_note_selection_region_view), note, chn_mask));
	} else {
		_view->foreach_selected_regionview (sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::add_note_selection_region_view), note, chn_mask));
	}
}

void
MidiTimeAxisView::extend_note_selection (uint8_t note)
{
	if (!_editor.internal_editing()) {
		return;
	}

	uint16_t chn_mask = _channel_selector.get_selected_channels();

	if (_view->num_selected_regionviews() == 0) {
		_view->foreach_regionview (sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::extend_note_selection_region_view), note, chn_mask));
	} else {
		_view->foreach_selected_regionview (sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::extend_note_selection_region_view), note, chn_mask));
	}
}

void
MidiTimeAxisView::toggle_note_selection (uint8_t note)
{
	if (!_editor.internal_editing()) {
		return;
	}

	uint16_t chn_mask = _channel_selector.get_selected_channels();

	if (_view->num_selected_regionviews() == 0) {
		_view->foreach_regionview (sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::toggle_note_selection_region_view), note, chn_mask));
	} else {
		_view->foreach_selected_regionview (sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::toggle_note_selection_region_view), note, chn_mask));
	}
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
MidiTimeAxisView::set_channel_mode (ChannelMode, uint16_t)
{
	/* hide all automation tracks that use the wrong channel(s) and show all those that use
	   the right ones.
	*/

	uint16_t selected_channels = _channel_selector.get_selected_channels();
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
MidiTimeAxisView::add_region (framepos_t pos, framecnt_t length, bool commit)
{
	Editor* real_editor = dynamic_cast<Editor*> (&_editor);

	real_editor->begin_reversible_command (Operations::create_region);
        playlist()->clear_changes ();

	real_editor->snap_to (pos, 0);

	boost::shared_ptr<Source> src = _session->create_midi_source_for_session (view()->trackview().track().get(),
                                                                                  view()->trackview().track()->name());
	PropertyList plist;

	plist.add (ARDOUR::Properties::start, 0);
	plist.add (ARDOUR::Properties::length, length);
	plist.add (ARDOUR::Properties::name, PBD::basename_nosuffix(src->name()));

	boost::shared_ptr<Region> region = (RegionFactory::create (src, plist));

	playlist()->add_region (region, pos);
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
	uint16_t const chn_mask = _channel_selector.get_selected_channels ();
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
