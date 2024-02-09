/*
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <cstdlib>
#include <cmath>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include <gtkmm/separator.h>
#include <gtkmm/stock.h>

#include "pbd/error.h"
#include "pbd/ffs.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"
#include "pbd/basename.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/unwind.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

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
#include "ardour/velocity_control.h"

#include "ardour_message.h"
#include "automation_line.h"
#include "automation_time_axis.h"
#include "editor.h"
#include "enums.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_channel_selector.h"
#include "midi_streamview.h"
#include "midi_region_view.h"
#include "midi_time_axis.h"
#include "patch_change_dialog.h"
#include "patch_change_widget.h"
#include "piano_roll_header.h"
#include "playlist_selector.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "point_selection.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "step_editor.h"
#include "note_base.h"

#include "ardour/midi_track.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using namespace std;

// Minimum height at which a control is displayed
static const uint32_t MIDI_CONTROLS_BOX_MIN_HEIGHT = 160;
static const uint32_t KEYBOARD_MIN_HEIGHT = 130;

#define DEFAULT_MIDNAM_MODEL (X_("Generic"))

MidiTimeAxisView::MidiTimeAxisView (PublicEditor& ed, Session* sess, ArdourCanvas::Canvas& canvas)
	: SessionHandlePtr (sess)
	, RouteTimeAxisView (ed, sess, canvas)
	, _ignore_signals(false)
	, _asked_all_automation(false)
	, _piano_roll_header(nullptr)
	, _note_mode(Sustained)
	, _note_mode_item(0)
	, _percussion_mode_item(nullptr)
	, _color_mode(MeterColors)
	, _meter_color_mode_item(nullptr)
	, _channel_color_mode_item(nullptr)
	, _track_color_mode_item(0)
	, _channel_selector (nullptr)
	, _step_edit_item (nullptr)
	, controller_menu (nullptr)
	, _step_editor (nullptr)
	, velocity_menu_item (nullptr)
{
	_midnam_model_selector.disable_scrolling();
	_midnam_custom_device_mode_selector.disable_scrolling();
	_midnam_channel_selector.disable_scrolling();

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &MidiTimeAxisView::parameter_changed));

	_editor.MouseModeChanged.connect_same_thread (mouse_mode_connection, sigc::mem_fun (*this, &MidiTimeAxisView::mouse_mode_changed));
}

void
MidiTimeAxisView::mouse_mode_changed ()
{
	_piano_roll_header->queue_resize ();
}

void
MidiTimeAxisView::parameter_changed (string const & param)
{
	if (param == X_("note-name-display")) {
		if (_piano_roll_header) {
			_piano_roll_header->instrument_info_change ();
		}
	}
}

void
MidiTimeAxisView::set_note_highlight (uint8_t note)
{
	if (_piano_roll_header) {
		_piano_roll_header->set_note_highlight (note);
	}
}

void
MidiTimeAxisView::set_route (std::shared_ptr<Route> rt)
{
	_route = rt;

	_view = new MidiStreamView (*this);

	if (is_track ()) {
		_piano_roll_header = new PianoRollHeader(*midi_view());
	}

	/* This next call will result in our height being set up, so it must come after
	 * the creation of the piano roll / range scroomer as their visibility is set up
	 * when our height is.
	 */
	RouteTimeAxisView::set_route (rt);

	_view->apply_color (Gtkmm2ext::gdk_color_to_rgba (color()), StreamView::RegionColor);

	_note_range_changed_connection.disconnect();

	if (!gui_property ("note-range-min").empty ()) {
		midi_view()->apply_note_range (atoi (gui_property ("note-range-min").c_str()),
		                               atoi (gui_property ("note-range-max").c_str()),
		                               true);
	}

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

	if (automation_child (MidiVelocityAutomation) == 0) {
		create_automation_child (MidiVelocityAutomation, false);
	}

	if (_route->panner_shell()) {
		_route->panner_shell()->Changed.connect (*this, invalidator (*this), boost::bind (&MidiTimeAxisView::ensure_pan_views, this, false), gui_context());
	}

	/* map current state of the route */
	ensure_pan_views (false);
	update_control_names();
	processors_changed (RouteProcessorChange ());

	if (is_track()) {
		_piano_roll_header->SetNoteSelection.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::set_note_selection));
		_piano_roll_header->AddNoteSelection.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::add_note_selection));
		_piano_roll_header->ExtendNoteSelection.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::extend_note_selection));
		_piano_roll_header->ToggleNoteSelection.connect (
			sigc::mem_fun (*this, &MidiTimeAxisView::toggle_note_selection));

		/* Put the scroomer and the keyboard in a VBox with a padding
		 * label so that they can be reduced in height for stacked-view
		 * tracks.
		 */

		HSeparator* separator = manage (new HSeparator());
		separator->set_name("TrackSeparator");
		separator->set_size_request(-1, 1);
		separator->show();

		VBox* v = manage (new VBox);
		HBox* h = manage (new HBox);
		h->pack_end (*_piano_roll_header);
		v->pack_start (*separator, false, false);
		v->pack_start (*h, true, true);
		v->show ();
		h->show ();
		top_hbox.remove(scroomer_placeholder);
		time_axis_hbox.pack_end(*v, false, false, 0);
		midi_scroomer_size_group->add_widget (*v);

		/* callback from StreamView scroomer drags, as well as
		 * automatic changes of note-range (e.g. at rec-stop).
		 * This callback is used to save the note-range-min/max
		 * GUI Object property
		 */
		_note_range_changed_connection = midi_view()->NoteRangeChanged.connect (
				sigc::mem_fun (*this, &MidiTimeAxisView::note_range_changed));

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (
			sigc::mem_fun(*this, &MidiTimeAxisView::region_view_added));

		if (!_editor.have_idled()) {
			/* first idle will do what we need */
		} else {
			first_idle ();
		}
	}

	ArdourWidgets::set_tooltip (_midnam_model_selector, _("External MIDI Device"));
	ArdourWidgets::set_tooltip (_midnam_custom_device_mode_selector, _("External Device Mode"));
	ArdourWidgets::set_tooltip (_midnam_channel_selector, _("MIDNAM Channel Display"));

	_midi_controls_box.pack_start (_midnam_model_selector, false, false, 2);
	_midi_controls_box.pack_start (_midnam_custom_device_mode_selector, false, false, 2);
	_midi_controls_box.pack_start (_midnam_channel_selector, false, false, 2);

	_midi_controls_box.set_homogeneous(false);
	_midi_controls_box.set_border_width (2);

	/* this directly calls use_midnam_info() if there are midnam's already */
	MIDI::Name::MidiPatchManager::instance().maybe_use (*this, invalidator (*this), boost::bind (&MidiTimeAxisView::use_midnam_info, this), gui_context());

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

		bool const p = AutomationTimeAxisView::parse_state_id (*i, route_id, has_parameter, parameter);
		if (p && route_id == _route->id () && has_parameter) {
			const std::string& visible = gui_object_state().get_string (*i, X_("visible"));
			create_automation_child (parameter, string_to<bool> (visible));
		}
	}

	//Menu_Helpers::MenuElem elem = Gtk::Menu_Helpers::MenuElem(_("Plugin Provided"),
	//				sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::model_changed),
	//					model_name));

	for (int i = 1; i < 17; i++){
		std::string text = string_compose ("%1 %2", _("Channel "), i);
		_midnam_channel_selector.append_text_item (text);
	}
	_midnam_channel_selector.StateChanged.connect (sigc::mem_fun (*this, &MidiTimeAxisView::_midnam_channel_changed));
	if (gui_property (X_("midnam-channel")).empty()) {
		std::string str = string_compose ("%1 1", _("Channel "));
		set_gui_property (X_("midnam-channel"), str);
		_midnam_channel_selector.set_active (str);
	} else {
		_midnam_channel_selector.set_active (gui_property (X_("midnam-channel")));
	}
}

void
MidiTimeAxisView::_midnam_channel_changed ()
{
	set_gui_property (X_("midnam-channel"), _midnam_channel_selector.get_text());
	//std::cout << "midnam_changed(): " << _midnam_channel_selector.get_text() << std::endl;
	_piano_roll_header->instrument_info_change ();
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
	delete _view;
	_view = nullptr;

	delete _channel_selector;
	_channel_selector = nullptr;

	delete _piano_roll_header;
	_piano_roll_header = nullptr;

	delete controller_menu;
	controller_menu = nullptr;

	delete _step_editor;
	_step_editor = nullptr;
}

void
MidiTimeAxisView::check_step_edit ()
{
	ensure_step_editor ();
	_step_editor->check_step_edit ();
}

void
MidiTimeAxisView::processors_changed (RouteProcessorChange c)
{
	RouteTimeAxisView::processors_changed (c);
	maybe_trigger_model_change ();
	update_patch_selector ();
}

void
MidiTimeAxisView::use_midnam_info ()
{
	/* Rebuild controller menu */
	_controller_menu_map.clear ();
	delete controller_menu;
	controller_menu = 0;

	setup_midnam_patches ();

	/* update names on any automation lane with MIDNAM names */
	for (AutomationTracks::iterator i = _automation_tracks.begin(); i != _automation_tracks.end(); ++i) {
		switch (i->first.type()) {
			case MidiCCAutomation:
				i->second->update_name_from_param ();
				break;
			default:
				continue;
		}
	}
}

void
MidiTimeAxisView::maybe_trigger_model_change ()
{
	if (_route->instrument_info().have_custom_plugin_info ()) {
		/* ensure that "Plugin Provided" is at the top of the list */
		if (_midnam_model_selector.items().empty () || _midnam_model_selector.items().begin()->get_label() != _("Plugin Provided")) {
			/* setup_midnam_patches unconditionally calls model_changed() */
			setup_midnam_patches ();
		}
	} else {
		/* no plugin provided MIDNAM for this plugin */
		if (!_midnam_model_selector.items().empty () && _midnam_model_selector.items().begin()->get_label() == _("Plugin Provided")) {
			/* setup_midnam_patches unconditionally calls model_changed() */
			setup_midnam_patches ();
		}
	}
}

void
MidiTimeAxisView::setup_midnam_patches ()
{
	typedef MIDI::Name::MidiPatchManager PatchManager;
	PatchManager& patch_manager = PatchManager::instance();

	_midnam_model_selector.clear_items ();

	if (_route->instrument_info().have_custom_plugin_info ()) {
		Menu_Helpers::MenuElem elem = Gtk::Menu_Helpers::MenuElem(_("Plugin Provided"), sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::model_changed), ""));
		_midnam_model_selector.AddMenuElem(elem);
	}

	for (PatchManager::DeviceNamesByMaker::const_iterator m = patch_manager.devices_by_manufacturer().begin(); m != patch_manager.devices_by_manufacturer().end(); ++m) {
		Menu*                   menu  = Gtk::manage(new Menu);
		Menu_Helpers::MenuList& items = menu->items();

		/* Build manufacturer submenu */
		for (MIDI::Name::MIDINameDocument::MasterDeviceNamesList::const_iterator n = m->second.begin(); n != m->second.end(); ++n) {

			if (patch_manager.is_custom_model (n->first)) {
				continue;
			}

			Menu_Helpers::MenuElem elem = Gtk::Menu_Helpers::MenuElem(
					n->first.c_str(),
					sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::model_changed),
						n->first.c_str()));

			items.push_back(elem);
		}
		if (items.empty ()) {
			delete menu;
			continue;
		}

		/* Add manufacturer submenu to selector */
		_midnam_model_selector.AddMenuElem(Menu_Helpers::MenuElem(m->first, *menu));
	}

	if (patch_manager.all_models().empty()) {
		_midnam_model_selector.hide ();
		_midnam_custom_device_mode_selector.hide ();
	} else {
		_midnam_model_selector.show ();
		if (_midnam_custom_device_mode_selector.items().size() > 1) {
			_midnam_custom_device_mode_selector.show ();
		}
	}

	/* call _midnam_model_selector.set_text ()
	 * and show/hide _midnam_custom_device_mode_selector
	 */
	std::string model = gui_property (X_("midnam-model-name"));
	if (model.empty() && _route->instrument_info().have_custom_plugin_info ()) {
		/* use plugin's MIDNAM */
		model_changed ("");
	} else if (model.empty() || ! MIDI::Name::MidiPatchManager::instance ().master_device_by_model (model)) {
		/* invalid model, switch to use default */
		model_changed ("");
	} else {
		model_changed (model);
	}

	_piano_roll_header->instrument_info_change ();
}

void
MidiTimeAxisView::update_patch_selector ()
{
	typedef MIDI::Name::MidiPatchManager PatchManager;
	PatchManager& patch_manager = PatchManager::instance();

	if (_route) {
		std::shared_ptr<PluginInsert> pi = std::dynamic_pointer_cast<PluginInsert> (_route->the_instrument ());
		if (pi && pi->plugin ()->has_midnam ()) {
			std::string model_name = pi->plugin ()->midnam_model ();
			if (gui_property (X_("midnam-model-name")) != model_name) {
				/* ensure that "Plugin Provided" is prefixed at the top of the list */
				if (_midnam_model_selector.items().empty () || _midnam_model_selector.items().begin()->get_label() != _("Plugin Provided")) {
					setup_midnam_patches ();
				}
			}
		}
	}

	if (patch_manager.all_models().empty()) {
		_midnam_model_selector.hide ();
		_midnam_custom_device_mode_selector.hide ();
		_midnam_channel_selector.hide ();
	} else {
		_midnam_model_selector.show ();
		_midnam_channel_selector.show ();
		if (_midnam_custom_device_mode_selector.items().size() > 1) {
			_midnam_custom_device_mode_selector.show ();
		}
	}
	_piano_roll_header->instrument_info_change ();

	/* call _midnam_model_selector.set_text ()
	 * and show/hide _midnam_custom_device_mode_selector
	 */
	std::string model = gui_property (X_("midnam-model-name"));
	if (model.empty() && _route->instrument_info().have_custom_plugin_info ()) {
		/* use plugin's MIDNAM */
		model_changed ("");
	} else if (model.empty() || ! MIDI::Name::MidiPatchManager::instance ().master_device_by_model (model)) {
		/* invalid model, switch to use default */
		model_changed ("");
	} else {
		model_changed (model);
	}
}

void
MidiTimeAxisView::model_changed (const std::string& m)
{
	typedef MIDI::Name::MidiPatchManager PatchManager;
	PatchManager& patch_manager = PatchManager::instance();

	std::string model (m);
	bool save_model = m != "";

	if (model.empty() && !_route->instrument_info().have_custom_plugin_info ()) {
		model = DEFAULT_MIDNAM_MODEL;
	}

	std::list<std::string> device_modes = patch_manager.custom_device_mode_names_by_model (model);

	if (device_modes.empty()) {
		save_model = false;
		if (_route->instrument_info().have_custom_plugin_info ()) {
			model = "";
		} else {
			model = DEFAULT_MIDNAM_MODEL;
		}
		device_modes = patch_manager.custom_device_mode_names_by_model (model);
	}

	if (model == "") {
		_midnam_model_selector.set_text (_("Plugin Provided"));
	} else {
		_midnam_model_selector.set_text (model);
	}

	if (save_model) {
		set_gui_property (X_("midnam-model-name"), model);
	} else {
		remove_gui_property (X_("midnam-model-name"));
	}

	/* set new mode */
	const std::string current_mode = gui_property (X_("midnam-custom-device-mode"));
	std::string mode;
	if (find (device_modes.begin(), device_modes.end(), current_mode) == device_modes.end()) {
		if (device_modes.size() > 0) {
			mode = device_modes.front();
			if (save_model) {
				custom_device_mode_changed (device_modes.front());
			}
		} else {
			mode = "";
		}
	} else {
		mode = current_mode;
	}

	/* set backend state */
	_route->instrument_info().set_external_instrument (model, mode);

	/* query effective model/mode -- may be plugin provided */
	if (_effective_model == _route->instrument_info().model () && _effective_mode == _route->instrument_info().mode ()) {
		/* no change */
		return;
	}

	_effective_model = _route->instrument_info().model ();
	_effective_mode  = _route->instrument_info().mode ();

	/* update GUI */

	_midnam_custom_device_mode_selector.clear_items();
	for (std::list<std::string>::const_iterator i = device_modes.begin(); i != device_modes.end(); ++i) {
		_midnam_custom_device_mode_selector.AddMenuElem (Gtk::Menu_Helpers::MenuElem(*i, sigc::bind(sigc::mem_fun(*this, &MidiTimeAxisView::custom_device_mode_changed), *i)));
	}

	if (device_modes.size() > 1) {
		_midnam_custom_device_mode_selector.show();
	} else {
		_midnam_custom_device_mode_selector.hide();
	}

	/* set _midnam_custom_device_mode_selector */
	custom_device_mode_changed (mode);

	/* Rebuild controller menu */
	_controller_menu_map.clear ();
	delete controller_menu;
	controller_menu = 0;

	if (patch_change_dialog ()) {
		patch_change_dialog ()->refresh ();
	}

	_piano_roll_header->instrument_info_change ();
}

void
MidiTimeAxisView::custom_device_mode_changed(const std::string& mode)
{
	const std::string model = gui_property (X_("midnam-model-name"));
	set_gui_property (X_("midnam-custom-device-mode"), mode);
	_midnam_custom_device_mode_selector.set_text (mode);
	if (model.empty () && !mode.empty ()) {
		/* model.empty () && model.empty () -> plugin provided
		 * otherwise at least a model must be set. */
		return;
	}
	/* inform the backend, route owned instrument info */
	_route->instrument_info().set_external_instrument (model, mode);

	_piano_roll_header->instrument_info_change ();
}

MidiStreamView*
MidiTimeAxisView::midi_view()
{
	return dynamic_cast<MidiStreamView*>(_view);
}

void
MidiTimeAxisView::update_midi_controls_visibility (uint32_t h)
{
	if (_route && !_route->active ()) {
		h = 0;
	}
	if (h >= MIDI_CONTROLS_BOX_MIN_HEIGHT) {
		_midi_controls_box.show ();
	} else {
		_midi_controls_box.hide();
	}
}

void
MidiTimeAxisView::set_height (uint32_t h, TrackHeightMode m, bool from_idle)
{
	update_midi_controls_visibility (h);

	if (h >= MIDI_CONTROLS_BOX_MIN_HEIGHT) {
		_midi_controls_box.show ();
	} else {
		_midi_controls_box.hide();
	}

	update_scroomer_visbility (h, layer_display ());

	/* We need to do this after changing visibility of our stuff, as it will
	 * eventually trigger a call to Editor::reset_controls_layout_width(),
	 * which needs to know if we have just shown or hidden a scroomer /
	 * piano roll.
	 */
	RouteTimeAxisView::set_height (h, m, from_idle);
}

void
MidiTimeAxisView::update_scroomer_visbility (uint32_t h, LayerDisplay d)
{
	if (!is_track ()) {
		return;
	}
	if (h >= KEYBOARD_MIN_HEIGHT && d == Overlaid) {
		if (_piano_roll_header) {
			_piano_roll_header->show();
		}
	} else {
		if (_piano_roll_header) {
			_piano_roll_header->hide();
		}
	}
}

void
MidiTimeAxisView::set_layer_display (LayerDisplay d)
{
	LayerDisplay prev_layer_display = layer_display ();
	RouteTimeAxisView::set_layer_display (d);
	LayerDisplay curr_layer_display = layer_display ();

	if (curr_layer_display == prev_layer_display) {
		return;
	}

	uint32_t h = current_height ();
	update_scroomer_visbility (h, curr_layer_display);

	/* If visibility changed, trigger a call to Editor::reset_controls_layout_width()
	 * by forcing a redraw via Editor::queue_redisplay_track_views ()
	 */
	_stripable->gui_changed ("visible_tracks", (void *) 0); /* EMIT SIGNAL */
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
		          sigc::bind (sigc::mem_fun(*this, &MidiTimeAxisView::set_visibility_note_range),
		                      MidiStreamView::FullRange, true)));

	range_items.push_back (
		MenuElem (_("Fit Contents"),
		          sigc::bind (sigc::mem_fun(*this, &MidiTimeAxisView::set_visibility_note_range),
		                      MidiStreamView::ContentsRange, true)));

	items.push_back (MenuElem (_("Note Range"), *range_menu));
	items.push_back (MenuElem (_("Note Mode"), *build_note_mode_menu()));
	items.push_back (MenuElem (_("Channel Selector..."),
				   sigc::mem_fun(*this, &MidiTimeAxisView::toggle_channel_selector)));

	items.push_back (MenuElem (_("Patch Selector..."),
				sigc::mem_fun(*this, &RouteUI::select_midi_patch)));

	items.push_back (CheckMenuElem (_("Restore Patch")));
	Gtk::CheckMenuItem* cmi = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
	cmi->set_active (midi_track ()->restore_pgm_on_load ());
	cmi->signal_activate().connect (sigc::mem_fun (*this, &MidiTimeAxisView::toggle_restore_pgm_on_load));

	items.push_back (MenuElem (_("Color Mode"), *build_color_mode_menu ()));

	items.push_back (SeparatorElem ());
}

void
MidiTimeAxisView::toggle_restore_pgm_on_load ()
{
	midi_track ()->set_restore_pgm_on_load (!midi_track ()->restore_pgm_on_load ());
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
	 * RouteTimeAxis::build_automation_action_menu destroys the
	 * menu it is attached to.  Otherwise GTK destroys
	 * controller_menu's gobj, meaning that it can't be reattached
	 * below.  See bug #3134.
	 */

	if (controller_menu) {
		detach_menu (*controller_menu);
	}

	_channel_command_menu_map.clear ();
	RouteTimeAxisView::build_automation_action_menu (for_selection);

	MenuList& automation_items = automation_action_menu->items();

	uint16_t selected_channels = midi_track()->get_playback_channel_mask();

	automation_items.push_back (CheckMenuElem (_("Velocity"), sigc::bind (sigc::mem_fun (*this, &MidiTimeAxisView::toggle_automation_track), MidiVelocityAutomation)));
	velocity_menu_item = dynamic_cast<Gtk::CheckMenuItem*> (&automation_items.back ());
	velocity_menu_item->set_active (string_to<bool> (velocity_track->gui_property ("visible")));

	if (selected_channels != 0) {

		automation_items.push_back (SeparatorElem());

		/* these 2 MIDI "command" types are semantically more like automation
		 * than note data, but they are not MIDI controllers. We give them
		 * special status in this menu, since they will not show up in the
		 * controller list and anyone who actually knows something about MIDI
		 * (!) would not expect to find them there.
		 */

		add_channel_command_menu_item (automation_items, _("Bender"), MidiPitchBenderAutomation, 0);
		automation_items.back().set_sensitive (!for_selection || _editor.get_selection().tracks.size() == 1);

		add_channel_command_menu_item (automation_items, _("Pressure"), MidiChannelPressureAutomation, 0);
		automation_items.back().set_sensitive (!for_selection || _editor.get_selection().tracks.size() == 1);

		/* now all MIDI controllers. Always offer the possibility that we will
		 * rebuild the controllers menu since it might need to be updated after
		 * a channel mode change or other change. Also detach it first in case
		 * it has been used anywhere else.
		 */
		build_controller_menu ();
		automation_items.push_back (MenuElem (_("Controllers"), *controller_menu));

		add_channel_command_menu_item (automation_items, _("Polyphonic Pressure"), MidiNotePressureAutomation, 0);
		automation_items.back().set_sensitive (!for_selection || _editor.get_selection().tracks.size() == 1);

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
	 * structure if there is more than 1 selected.
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

				std::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);
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

				std::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);
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

			std::shared_ptr<AutomationTimeAxisView> track = automation_child (
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
                                                    const uint16_t          channels,
                                                    int                     ctl,
                                                    const std::string&      name)
{
	using namespace Menu_Helpers;

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
		if (channels & (0x0001 << chn)) {

			/* for each selected channel, add a menu item for this controller */

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			chn_items.push_back (
				CheckMenuElem (string_compose (_("Channel %1"), chn+1),
				               sigc::bind (sigc::mem_fun (*this, &RouteTimeAxisView::toggle_automation_track),
				                           fully_qualified_param)));

			std::shared_ptr<AutomationTimeAxisView> track = automation_child (
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
	 * time), and populate each one with a submenu for each controller+channel
	 * combination covering the currently selected channels for this track
	 */

	size_t total_ctrls = _route->instrument_info().master_controller_count ();
	if (total_ctrls > 0) {
		/* Controllers names available in midnam file, generate fancy menu */
		using namespace MIDI::Name;

		unsigned n_items  = 0;
		unsigned n_groups = 0;

		/* keep track of CC numbers that are added */
		uint16_t ctl_start = 1;
		uint16_t ctl_end   = 1;

		MasterDeviceNames::ControlNameLists const& ctllist (_route->instrument_info().master_device_names ()->controls ());

		bool per_name_list = ctllist.size () > 1;
		bool to_top_level = total_ctrls < 32 && !per_name_list;

		/* reverse lookup which "ChannelNameSet" has "UsesControlNameList <this list>"
		 * then check for which channels it is valid "AvailableForChannels"
		 */

		for (MasterDeviceNames::ControlNameLists::const_iterator l = ctllist.begin(); l != ctllist.end(); ++l) {

			uint16_t channels  = _route->instrument_info().channels_for_control_list (l->first);
			bool multi_channel = 0 != (channels & (channels - 1));

			std::shared_ptr<ControlNameList> name_list = l->second;
			Menu*                              ctl_menu  = NULL;

			for (ControlNameList::Controls::const_iterator c = name_list->controls().begin();
			     c != name_list->controls().end();) {

				const uint16_t ctl = c->second->number();

				/* Skip bank select controllers since they're handled specially */
				if (ctl != MIDI_CTL_MSB_BANK && ctl != MIDI_CTL_LSB_BANK) {

					if (to_top_level) {
						ctl_menu = controller_menu;
					} else if (!ctl_menu) {
						/* Create a new submenu */
						ctl_menu = manage (new Menu);
						ctl_start = ctl;
					}

					MenuList& ctl_items (ctl_menu->items());
					if (multi_channel) {
						add_multi_channel_controller_item(ctl_items, channels, ctl, c->second->name());
					} else {
						add_single_channel_controller_item(ctl_items, ctl, c->second->name());
					}
					ctl_end = ctl;
				}

				++c;

				if (!ctl_menu || to_top_level) {
					continue;
				}

				if (++n_items == 32 || ctl < ctl_start || c == name_list->controls().end()) {
					/* Submenu has 32 items or we're done, or a new name-list started:
					 * add it to controller menu and reset */
					items.push_back (MenuElem (string_compose ("%1 %2-%3",
									(per_name_list ? l->first.c_str() : _("Controllers")),
									ctl_start, ctl_end), *ctl_menu));
					ctl_menu = NULL;
					n_items  = 0;
					++n_groups;
				}
			}
		}
	} else {
		/* No controllers names, generate generic numeric menu */

		const uint16_t selected_channels = midi_track()->get_playback_channel_mask();

		/* count the number of selected channels because we will build a different menu
		 * structure if there is more than 1 selected.
		 */

		int chn_cnt = 0;
		for (uint8_t chn = 0; chn < 16; chn++) {
			if (selected_channels & (0x0001 << chn)) {
				if (++chn_cnt > 1) {
					break;
				}
			}
		}

		for (int i = 0; i < 127; i += 32) {
			Menu*     ctl_menu = manage (new Menu);
			MenuList& ctl_items (ctl_menu->items());

			for (int ctl = i; ctl < i + 32; ++ctl) {
				if (ctl == MIDI_CTL_MSB_BANK || ctl == MIDI_CTL_LSB_BANK) {
					/* Skip bank select controllers since they're handled specially */
					continue;
				}

				if (chn_cnt > 1) {
					add_multi_channel_controller_item(
						ctl_items, selected_channels, ctl, string_compose(_("Controller %1"), ctl));
				} else {
					add_single_channel_controller_item(
						ctl_items, ctl, string_compose(_("Controller %1"), ctl));
				}
			}

			/* Add submenu for this block of controllers to controller menu */
			switch (i) {
				case 0:
				case 32:
					/* skip 0x00 and 0x20 (bank-select) */
					items.push_back (MenuElem (string_compose (_("Controllers %1-%2"), i + 1, i + 31), *ctl_menu));
					break;
				default:
					items.push_back (MenuElem (string_compose (_("Controllers %1-%2"), i, i + 31), *ctl_menu));
					break;
			}
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
MidiTimeAxisView::set_visibility_note_range (MidiStreamView::VisibleNoteRange range, bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (
			boost::bind (&MidiTimeAxisView::set_visibility_note_range, _1, range, false));
	} else {
		if (!_ignore_signals) {
			midi_view()->set_note_visibility_range_style (range);
		}
	}
}

void
MidiTimeAxisView::show_all_automation (bool apply_to_selection)
{
	using namespace MIDI::Name;

	if (!_asked_all_automation) {
		ArdourMessageDialog md (_("Are you sure you want to\nshow all MIDI automation lanes?"), false, MESSAGE_QUESTION, BUTTONS_YES_NO);

		md.set_title (_("Show All Automation"));

		md.set_secondary_text (_("There are a total of 16 MIDI channels times 128 Control-Change parameters, not including other MIDI controls. Showing all will add more than 2000 automation lanes which is not generally useful. This will take some time and also slow down the GUI significantly."));
		if (md.run () != RESPONSE_YES) {
			return;
		}
	}

	PBD::Unwinder<bool> uw (_asked_all_automation, true);

	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_midi_time_axis (
			boost::bind (&MidiTimeAxisView::show_all_automation, _1, false));
	} else {
		no_redraw = true; // unset in RouteTimeAxisView::show_all_automation
		if (midi_track()) {
			// Show existing automation
			const set<Evoral::Parameter> params = midi_track()->midi_playlist()->contained_automation();

			for (set<Evoral::Parameter>::const_iterator i = params.begin(); i != params.end(); ++i) {
				create_automation_child(*i, true);
			}

			/* Show automation for all controllers named in midnam file */
			if (_route->instrument_info().master_controller_count () > 0) {

				const uint16_t selected_channels = midi_track()->get_playback_channel_mask();

				for (uint32_t chn = 0; chn < 16; ++chn) {

					if ((selected_channels & (0x0001 << chn)) == 0) {
						// Channel not in use
						continue;
					}

					std::shared_ptr<ControlNameList> control_names = _route->instrument_info().control_name_list (chn);
					if (!control_names) {
						continue;
					}

					for (ControlNameList::Controls::const_iterator c = control_names->controls().begin(); c != control_names->controls().end(); ++c) {
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

	std::shared_ptr<AutomationTimeAxisView> track;
	std::shared_ptr<AutomationControl> control;

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

	case MidiVelocityAutomation:
		create_velocity_automation_child (param, show);
		break;

	case MidiCCAutomation:
	case MidiPgmChangeAutomation:
	case MidiPitchBenderAutomation:
	case MidiChannelPressureAutomation:
	case MidiNotePressureAutomation:
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
			             control ? _route : std::shared_ptr<Automatable> (),
			             control,
			             param,
			             _editor,
			             *this,
			             true,
			             parent_canvas,
			             /* this calls MidiTrack::describe_parameter()
			              * -> instrument_info().get_controller_name()
			              */
			             _route->describe_parameter(param)));

		if (_view) {
			_view->foreach_regionview (sigc::mem_fun (*track.get(), &TimeAxisView::add_ghost));
		}

		add_automation_child (param, track, show);
		if (selected ()) {
			reshow_selection (_editor.get_selection().time);
		}

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
	RouteTimeAxisView::route_active_changed ();
	update_control_names();
	update_midi_controls_visibility (height);

	if (!_route->active()) {
		controls_table.hide();
		inactive_table.show();
		RouteTimeAxisView::hide_all_automation();
	} else {
		inactive_table.hide();
		controls_table.show();
	}
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

	/* set_note_selection_region_view() will not work with multiple regions,
	 * as each individual `foreach` call will clear prior selection.
	 * Use clear_midi_notes() and add_note_selection_region_view() instead. */

	_editor.get_selection().clear_midi_notes();

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
MidiTimeAxisView::get_per_region_note_selection (list<pair<PBD::ID, set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > > >& selection)
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
MidiTimeAxisView::get_per_region_note_selection_region_view (RegionView* rv, list<pair<PBD::ID, std::set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > > > &selection)
{
	Evoral::Sequence<Temporal::Beats>::Notes selected;
	dynamic_cast<MidiRegionView*>(rv)->selection_as_notelist (selected, false);

	std::set<std::shared_ptr<Evoral::Note<Temporal::Beats> > > notes;

	Evoral::Sequence<Temporal::Beats>::Notes::iterator sel_it;
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
	/* hide all automation tracks that use the wrong channel(s) and show all those thatcw
	 * use the right ones. */

	const uint16_t selected_channels = midi_track()->get_playback_channel_mask();
	bool changed = false;

	no_redraw = true;

	for (uint32_t ctl = 0; ctl < 127; ++ctl) {

		for (uint32_t chn = 0; chn < 16; ++chn) {
			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			std::shared_ptr<AutomationTimeAxisView> track = automation_child (fully_qualified_param);

			if (!track) {
				continue;
			}

			if ((selected_channels & (0x0001 << chn)) == 0) {
				/* channel not in use. hiding it will trigger RouteTimeAxisView::automation_track_hidden()
				 * which will cause a redraw. We don't want one per channel, so block that with no_redraw.
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

	if (param.type() == MidiVelocityAutomation) {
		return velocity_menu_item;
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

std::shared_ptr<MidiRegion>
MidiTimeAxisView::add_region (timepos_t const & f, timecnt_t const & length, bool commit)
{
	Editor* real_editor = dynamic_cast<Editor*> (&_editor);
	timepos_t pos (f);

	if (commit) {
		real_editor->begin_reversible_command (Operations::create_region);
	}
	playlist()->clear_changes ();

	real_editor->snap_to (pos, Temporal::RoundNearest);

	std::shared_ptr<Source> src = _session->create_midi_source_by_stealing_name (view()->trackview().track());

	const Temporal::timecnt_t start (Temporal::BeatTime); /* zero beats */

	std::shared_ptr<Region> region;

	/* Create the (empty) whole-file region that will show up in the source
	 * list. This is NOT used in any playlists.
	 */

	{
		PropertyList plist;

		plist.add (ARDOUR::Properties::start, start);
		plist.add (ARDOUR::Properties::length, length);
		plist.add (ARDOUR::Properties::automatic, true);
		plist.add (ARDOUR::Properties::whole_file, true);
		plist.add (ARDOUR::Properties::name, PBD::basename_nosuffix(src->name()));
		plist.add (ARDOUR::Properties::opaque, _session->config.get_draw_opaque_midi_regions());

		region = (RegionFactory::create (src, plist, true));
	}

	/* Now create the region that we will actually use within the playlist */

	{
		PropertyList plist;
		plist.add (ARDOUR::Properties::name, region->name());
		region = RegionFactory::create (region, plist, false);
	}

	region->set_position (pos);
	playlist()->add_region (region, pos, 1.0, false);
	_session->add_command (new StatefulDiffCommand (playlist()));

	if (commit) {
		real_editor->commit_reversible_command ();
	}

	return std::dynamic_pointer_cast<MidiRegion>(region);
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
MidiTimeAxisView::get_preferred_midi_channel () const
{
	if (_editor.draw_channel() != Editing::DRAW_CHAN_AUTO) {
		return _editor.draw_channel();
	}

	if (_midnam_channel_selector.is_visible()) {
		string chn = gui_property (X_("midnam-channel"));
		if (!chn.empty()) {
			int midnam_channel;
			sscanf (chn.c_str(), "%*s %d", &midnam_channel);
			midnam_channel--;
			return midnam_channel;
		}
	}

	uint16_t const chn_mask = midi_track()->get_playback_channel_mask();
	int chn_cnt = 0;
	uint8_t channel = 0;

	/* pick the highest selected channel, unless all channels are selected,
	 * which is interpreted to mean channel 1 (zero)
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

bool
MidiTimeAxisView::paste (timepos_t const & pos, const Selection& selection, PasteContext& ctx)
{
	if (!_editor.internal_editing()) {
		// Non-internal paste, paste regions like any other route
		return RouteTimeAxisView::paste (pos, selection, ctx);
	}

	return midi_view()->paste (pos, selection, ctx);
}

void
MidiTimeAxisView::get_regions_with_selected_data (RegionSelection& rs)
{
	midi_view()->get_regions_with_selected_data (rs);
}

void
MidiTimeAxisView::create_velocity_automation_child (Evoral::Parameter const &, bool show)
{
	std::shared_ptr<AutomationControl> c = midi_track()->velocity_control();

	if (!c) {
		error << "MidiTrack has no velocity automation, unable to add automation track view." << endmsg;
		return;
	}

	velocity_track.reset (new AutomationTimeAxisView (_session,
	                                                  _route, midi_track(), c, c->parameter(),
	                                                  _editor,
	                                                  *this,
	                                                  false,
	                                                  parent_canvas,
	                                                  midi_track()->describe_parameter(c->parameter())));

	if (_view) {
		_view->foreach_regionview (sigc::mem_fun (*velocity_track.get(), &TimeAxisView::add_ghost));
	}

	add_automation_child (Evoral::Parameter(MidiVelocityAutomation), velocity_track, show);
}
