/*
 * Copyright (C) 2016-2026 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#include <ytkmm/menu.h>

#include "pbd/string_convert.h"

#include "ardour/folder.h"
#include "ardour/mute_control.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/solo_control.h"

#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/utils.h"
#include "widgets/tooltips.h"

#include "folder_streamview.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "mixer_ui.h"
#include "ui_config.h"
#include "folder_time_axis.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

FolderTimeAxisView::FolderTimeAxisView (PublicEditor& ed, Session* s, ArdourCanvas::Canvas& canvas)
	: SessionHandlePtr (s)
	, StripableTimeAxisView (ed, s, canvas)
	, gain_meter (s, true, 75, 14) // XXX stupid magic numbers, match sizes in RouteTimeAxisView
	, _streamview (nullptr)
	, automation_action_menu (nullptr)
{
	controls_base_selected_name = X_("ControlMasterBaseSelected");
	controls_base_unselected_name = X_("ControlMasterBaseUnselected");

	solo_button.set_name ("solo button");
	set_tooltip (solo_button, _("Solo assigned channels"));
	solo_button.signal_button_release_event().connect (sigc::mem_fun (*this, &FolderTimeAxisView::solo_release), false);
	solo_button.set_can_focus (false);

	mute_button.set_name ("mute button");
	mute_button.set_text (S_("Mute|M"));
	set_tooltip (mute_button, _("Mute assigned channels"));
	mute_button.signal_button_release_event().connect (sigc::mem_fun (*this, &FolderTimeAxisView::mute_release), false);
	mute_button.set_can_focus (false);

	drop_button.set_name ("mute button");
	drop_button.set_text (S_("Folder|D"));
	set_tooltip (drop_button, _("Unassign all channels"));
	drop_button.signal_button_release_event().connect (sigc::mem_fun (*this, &FolderTimeAxisView::drop_release), false);
	drop_button.set_can_focus (false);

	automation_button.set_name ("route button");
	automation_button.set_text (S_("RTAV|A"));
	set_tooltip (automation_button, _("Automation"));
	automation_button.signal_button_press_event().connect (sigc::mem_fun (*this, &FolderTimeAxisView::automation_click), false);
	automation_button.set_can_focus (false);

	mute_button.set_tweaks(ArdourButton::TrackHeader);
	solo_button.set_tweaks(ArdourButton::TrackHeader);
	drop_button.set_tweaks(ArdourButton::TrackHeader);
	automation_button.set_tweaks(ArdourButton::TrackHeader);

	if (ARDOUR::Profile->get_mixbus()) {
		controls_button_size_group->add_widget(mute_button);

		Gtk::Fixed *blank = manage(new Gtk::Fixed());
		controls_button_size_group->add_widget(*blank);
		controls_table.attach (*blank, 0, 1, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		blank->show();

		controls_table.attach (mute_button, 1, 2, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (solo_button, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (automation_button, 1, 2, 2, 3, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (drop_button, 2, 3, 2, 3, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (gain_meter.get_gain_slider(), 3, 5, 2, 3, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 1, 0);
	} else {
		controls_table.attach (mute_button, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (solo_button, 3, 4, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (automation_button, 2, 3, 1, 2, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (drop_button, 3, 4, 1, 2, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
		controls_table.attach (gain_meter.get_gain_slider(), 0, 2, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 1, 0);
	}

	mute_button.show ();
	solo_button.show ();
	drop_button.show ();
	automation_button.show ();
	gain_meter.get_gain_slider().show ();

	controls_ebox.set_name (controls_base_unselected_name);
	time_axis_frame.set_name (controls_base_unselected_name);

	s->MonitorBusAddedOrRemoved.connect (*this, invalidator (*this), std::bind (&FolderTimeAxisView::set_button_names, this), gui_context());

	s->config.ParameterChanged.connect (*this, invalidator (*this), std::bind (&FolderTimeAxisView::parameter_changed, this, _1), gui_context());
	Config->ParameterChanged.connect (*this, invalidator (*this), std::bind (&FolderTimeAxisView::parameter_changed, this, _1), gui_context());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &FolderTimeAxisView::parameter_changed));
}

FolderTimeAxisView::~FolderTimeAxisView ()
{
	delete _streamview;
	delete automation_action_menu;
	CatchDeletion (this);
}

void
FolderTimeAxisView::self_delete ()
{
	/* reset reference immediately rather than deferring to idle */
	_folder.reset ();
	delete_when_idle (this);
}

void
FolderTimeAxisView::parameter_changed (std::string const & p)
{
	if (p == "track-name-number") {
		update_track_number_visibility();
	} else if (p == "solo-control-is-listen-control" || p == "listen-position") {
		set_button_names ();
	}
}

bool
FolderTimeAxisView::solo_release (GdkEventButton*)
{
	/* We use NoGroup because Folder controls are never part of a group. This
	   is redundant, but clear.
	*/
	_folder->solo_control()->set_value (_folder->solo_control()->self_soloed() ? 0.0 : 1.0, Controllable::NoGroup);
	return true;
}

bool
FolderTimeAxisView::mute_release (GdkEventButton*)
{
	/* We use NoGroup because Folder controls are never part of a group. This
	   is redundant, but clear.
	*/
	_folder->mute_control()->set_value (_folder->mute_control()->muted_by_self() ? 0.0 : 1.0, Controllable::NoGroup);
	return true;
}

void
FolderTimeAxisView::set_folder (std::shared_ptr<Folder> f)
{
	StripableTimeAxisView::set_stripable (f);
	_folder = f;

	delete _streamview;
	_streamview = new FolderStreamView (_editor, _canvas_display, _folder);

	gain_meter.set_controls (f,
	                         std::shared_ptr<PeakMeter>(),
	                         std::shared_ptr<Amp>(),
	                         _folder->gain_control());

	// Mixer_UI::instance()->show_folder_change.connect (sigc::mem_fun (*this, &FolderMasterStrip::spill_change));

	_folder->PropertyChanged.connect (folder_connections, invalidator (*this), std::bind (&FolderTimeAxisView::folder_property_changed, this, _1), gui_context());

	_folder->solo_control()->Changed.connect (folder_connections, invalidator (*this), std::bind (&FolderTimeAxisView::update_solo_display, this), gui_context());
	_folder->mute_control()->Changed.connect (folder_connections, invalidator (*this), std::bind (&FolderTimeAxisView::update_mute_display, this), gui_context());
	_folder->DropReferences.connect (folder_connections, invalidator (*this), std::bind (&FolderTimeAxisView::self_delete, this), gui_context());

	solo_button.set_controllable (_folder->solo_control());
	mute_button.set_controllable (_folder->mute_control());

	/* XXX set this somehow, perhaps */
	// number_label.set_text (PBD::to_string (_folder->number()));

	set_height (preset_height (HeightNormal));

	if (automation_child (GainAutomation) == 0) {
		create_automation_child (GainAutomation, false);
	}
	if (automation_child (MuteAutomation) == 0) {
		create_automation_child (MuteAutomation, false);
	}

	update_folder_name ();
	set_button_names ();
	update_solo_display ();
	update_mute_display ();
	update_track_number_visibility ();
}

void
FolderTimeAxisView::folder_property_changed (PropertyChange const & what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		update_folder_name ();
	}
}

void
FolderTimeAxisView::update_folder_name ()
{
	name_label.set_text (_folder->name());
}

bool
FolderTimeAxisView::name_entry_changed (std::string const& str)
{
	_folder->set_name (str);
	return true;
}

void
FolderTimeAxisView::update_mute_display ()
{
	if (_folder->mute_control()->muted_by_self()) {
		mute_button.set_active_state (ExplicitActive);
	} else if (_folder->mute_control()->muted_by_masters ()) {
		mute_button.set_active_state (ImplicitActive);
	} else {
		mute_button.set_active_state (Gtkmm2ext::Off);
	}
}

void
FolderTimeAxisView::update_solo_display ()
{
	if (_folder->solo_control()->self_soloed()) {
		solo_button.set_active_state (ExplicitActive);
	} else if (_folder->solo_control()->soloed_by_masters ()) {
		solo_button.set_active_state (ImplicitActive);
	} else {
		solo_button.set_active_state (Gtkmm2ext::Off);
	}

	update_mute_display ();
}

std::string
FolderTimeAxisView::name() const
{
	return _folder->name();
}

std::string
FolderTimeAxisView::state_id() const
{
	return string_compose ("vtv %1", _folder->id().to_s());
}

void
FolderTimeAxisView::set_button_names ()
{
	if (Config->get_solo_control_is_listen_control()) {
		switch (Config->get_listen_position()) {
		case AfterFaderListen:
			solo_button.set_text (S_("AfterFader|A"));
			set_tooltip (solo_button, _("After-fade listen (AFL)"));
			break;
		case PreFaderListen:
			solo_button.set_text (S_("PreFader|P"));
			set_tooltip (solo_button, _("Pre-fade listen (PFL)"));
			break;
		}
	} else {
		solo_button.set_text (S_("Solo|S"));
		set_tooltip (solo_button, _("Solo"));
	}
}

void
FolderTimeAxisView::update_track_number_visibility ()
{
	DisplaySuspender ds;
	bool show_label = _session->config.get_track_name_number();

	if (number_label.get_parent()) {
		controls_table.remove (number_label);
	}

	if (show_label) {
		if (ARDOUR::Profile->get_mixbus()) {
			controls_table.attach (number_label, 3, 4, 0, 1, Gtk::SHRINK, Gtk::EXPAND|Gtk::FILL, 1, 0);
		} else {
			controls_table.attach (number_label, 0, 1, 0, 1, Gtk::SHRINK, Gtk::EXPAND|Gtk::FILL, 1, 0);
		}

		// see ArdourButton::on_size_request(), we should probably use a global size-group here instead.
		// except the width of the number label is subtracted from the name-hbox, so we
		// need to explicitly calculate it anyway until the name-label & entry become ArdourWidgets.

		int tnw = (2 + std::max(2u, _session->track_number_decimals())) * number_label.char_pixel_width();
		if (tnw & 1) --tnw;
		number_label.set_size_request(tnw, -1);
		number_label.show ();
	} else {
		number_label.hide ();
	}
}

bool
FolderTimeAxisView::automation_click (GdkEventButton* ev)
{
	if (ev->button != 1) {
		return true;
	}

	conditionally_add_to_selection ();
	build_automation_action_menu (false);
	Gtkmm2ext::anchored_menu_popup (automation_action_menu, &automation_button, "", 1, ev->time);
	return true;
}

bool
FolderTimeAxisView::drop_release (GdkEventButton*)
{
	// XXX implement this 
	// _folder->Drop (); /* EMIT SIGNAL */

	return true;
}

PresentationInfo const &
FolderTimeAxisView::presentation_info () const
{
	return _folder->presentation_info();
}

std::shared_ptr<Stripable>
FolderTimeAxisView::stripable () const
{
	return _folder;
}

Gdk::Color
FolderTimeAxisView::color () const
{
	return Gtkmm2ext::gdk_color_from_rgba (_folder->presentation_info().color ());
}

void
FolderTimeAxisView::set_height (uint32_t h, TrackHeightMode m, bool from_idle)
{
	TimeAxisView::set_height (h, m, from_idle);

	if (height >= preset_height (HeightNormal)) {
		drop_button.show ();
		automation_button.show ();
		gain_meter.get_gain_slider().show ();
	} else {
		drop_button.hide ();
		automation_button.hide ();
		gain_meter.get_gain_slider().hide ();
	}

	std::cerr << "FTAV, sv = " << _streamview << std::endl;

	if (_streamview) {
		_streamview->set_height (h);
	}

	set_gui_property ("height", h);
	_folder->gui_changed ("track_height", (void*) 0); /* EMIT SIGNAL */
}

bool
FolderTimeAxisView::marked_for_display () const
{
	return _folder && !_folder->presentation_info().hidden();
}

bool
FolderTimeAxisView::set_marked_for_display (bool yn)
{
	if (_folder && (yn == _folder->presentation_info().hidden())) {
		_folder->presentation_info().set_hidden (!yn);
		return true; // things changed
	}
	return false;
}

void
FolderTimeAxisView::create_gain_automation_child (const Evoral::Parameter& param, bool show)
{
	std::shared_ptr<AutomationControl> c = _folder->gain_control();
	if (!c) {
		error << "Folder has no gain automation, unable to add automation track view." << endmsg;
		return;
	}

	gain_track.reset (new AutomationTimeAxisView (_session,
						      _folder, std::shared_ptr<Automatable> (), c, param,
						      _editor,
						      *this,
						      false,
						      parent_canvas,
						      /*_route->amp()->describe_parameter(param)*/"Fader"));

	add_automation_child (Evoral::Parameter(GainAutomation), gain_track, show);
}

void
FolderTimeAxisView::create_mute_automation_child (const Evoral::Parameter& param, bool show)
{
	std::shared_ptr<AutomationControl> c = _folder->mute_control();
	if (!c) {
		error << "Folder has no mute automation, unable to add automation track view." << endmsg;
		return;
	}

	mute_track.reset (new AutomationTimeAxisView (_session,
						      _folder, std::shared_ptr<Automatable> (), c, param,
						      _editor,
						      *this,
						      false,
						      parent_canvas,
						      /*_route->describe_parameter(param)*/ "Mute"));

	add_automation_child (Evoral::Parameter(MuteAutomation), mute_track, show);
}

void
FolderTimeAxisView::create_automation_child (const Evoral::Parameter& param, bool show)
{
	switch (param.type()) {
		case GainAutomation:
			create_gain_automation_child (param, show);
			break;
		case MuteAutomation:
			create_mute_automation_child (param, show);
			break;
		default:
			break;
	}
}

void
FolderTimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;
	/* prepare it */
	TimeAxisView::build_display_menu ();

	MenuList& items = display_menu->items();
	items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &FolderTimeAxisView::choose_color)));

	build_size_menu ();
	items.push_back (MenuElem (_("Height"), *_size_menu));
	items.push_back (SeparatorElem());

	build_automation_action_menu (true);
	items.push_back (MenuElem (_("Automation"), *automation_action_menu));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Drop All Assigned Channels"), sigc::mem_fun (*this, &FolderTimeAxisView::drop_all_slaves)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(_editor, &PublicEditor::remove_tracks)));
}


void
FolderTimeAxisView::build_automation_action_menu (bool for_selection)
{
	using namespace Menu_Helpers;
	_main_automation_menu_map.clear ();
	delete automation_action_menu;
	automation_action_menu = new Menu;

	MenuList& items = automation_action_menu->items();

	automation_action_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All Automation"),
	                           sigc::bind (sigc::mem_fun (*this, &FolderTimeAxisView::show_all_automation), for_selection)));

	items.push_back (MenuElem (_("Show Existing Automation"),
	                           sigc::bind (sigc::mem_fun (*this, &FolderTimeAxisView::show_existing_automation), for_selection)));

	items.push_back (MenuElem (_("Hide All Automation"),
	                           sigc::bind (sigc::mem_fun (*this, &FolderTimeAxisView::hide_all_automation), for_selection)));

	if (gain_track) {
		items.push_back (CheckMenuElem (_("Fader"), sigc::mem_fun (*this, &FolderTimeAxisView::update_gain_track_visibility)));
		gain_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		gain_automation_item->set_active (string_to<bool>(gain_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(GainAutomation)] = gain_automation_item;
	}

	if (trim_track) {
		items.push_back (CheckMenuElem (S_("Gain|Trim"), sigc::mem_fun (*this, &FolderTimeAxisView::update_trim_track_visibility)));
		trim_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		trim_automation_item->set_active (string_to<bool>(trim_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(TrimAutomation)] = trim_automation_item;
	}

	if (mute_track) {
		items.push_back (CheckMenuElem (_("Mute"), sigc::mem_fun (*this, &FolderTimeAxisView::update_mute_track_visibility)));
		mute_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		mute_automation_item->set_active (string_to<bool>(mute_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(MuteAutomation)] = mute_automation_item;
	}
}

void
FolderTimeAxisView::show_all_automation (bool apply_to_selection)
{
	assert (!apply_to_selection); // Folders can't yet be selected
	no_redraw = true;

	StripableTimeAxisView::show_all_automation ();

	no_redraw = false;
	request_redraw ();
}

void
FolderTimeAxisView::show_existing_automation (bool apply_to_selection)
{
	assert (!apply_to_selection); // Folders can't yet be selected
	no_redraw = true;

	StripableTimeAxisView::show_existing_automation ();

	no_redraw = false;
	request_redraw ();
}

void
FolderTimeAxisView::hide_all_automation (bool apply_to_selection)
{
	assert (!apply_to_selection); // Folders can't yet be selected
	no_redraw = true;

	StripableTimeAxisView::hide_all_automation ();

	no_redraw = false;
	request_redraw ();
}

void
FolderTimeAxisView::drop_all_slaves ()
{
	/* XXX implement this */
	// _folder->Drop (); /* EMIT SIGNAL */

	if (Mixer_UI::instance()->showing_spill_for (_folder)) {
		Mixer_UI::instance()->show_spill (std::shared_ptr<Stripable>());
	}
}

void
FolderTimeAxisView::choose_color ()
{
	StripableColorDialog* scd = _folder->active_color_picker();

	if (!scd) {
		scd = new StripableColorDialog (_folder);
	}

	scd->popup (dynamic_cast<Gtk::Window*> (PublicEditor::instance ().current_toplevel()));
}
