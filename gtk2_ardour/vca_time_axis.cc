/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gtkmm/menu.h>

#include "pbd/string_convert.h"

#include "ardour/mute_control.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/solo_control.h"
#include "ardour/vca.h"

#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/utils.h"
#include "widgets/tooltips.h"

#include "gui_thread.h"
#include "public_editor.h"
#include "mixer_ui.h"
#include "ui_config.h"
#include "vca_time_axis.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

VCATimeAxisView::VCATimeAxisView (PublicEditor& ed, Session* s, ArdourCanvas::Canvas& canvas)
	: SessionHandlePtr (s)
	, StripableTimeAxisView (ed, s, canvas)
	, gain_meter (s, true, 75, 14) // XXX stupid magic numbers, match sizes in RouteTimeAxisView
	, automation_action_menu (0)
{

	controls_base_selected_name = X_("ControlMasterBaseSelected");
	controls_base_unselected_name = X_("ControlMasterBaseUnselected");

	solo_button.set_name ("solo button");
	set_tooltip (solo_button, _("Solo slaves"));
	solo_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCATimeAxisView::solo_release), false);
	solo_button.unset_flags (Gtk::CAN_FOCUS);

	mute_button.set_name ("mute button");
	mute_button.set_text (S_("Mute|M"));
	set_tooltip (mute_button, _("Mute slaves"));
	mute_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCATimeAxisView::mute_release), false);
	mute_button.unset_flags (Gtk::CAN_FOCUS);

	drop_button.set_name ("mute button");
	drop_button.set_text (S_("VCA|D"));
	set_tooltip (drop_button, _("Unassign all slaves"));
	drop_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCATimeAxisView::drop_release), false);
	drop_button.unset_flags (Gtk::CAN_FOCUS);

	automation_button.set_name ("route button");
	automation_button.set_text (S_("RTAV|A"));
	set_tooltip (automation_button, _("Automation"));
	automation_button.signal_button_press_event().connect (sigc::mem_fun (*this, &VCATimeAxisView::automation_click), false);
	automation_button.unset_flags (Gtk::CAN_FOCUS);

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

	s->MonitorBusAddedOrRemoved.connect (*this, invalidator (*this), boost::bind (&VCATimeAxisView::set_button_names, this), gui_context());

	s->config.ParameterChanged.connect (*this, invalidator (*this), boost::bind (&VCATimeAxisView::parameter_changed, this, _1), gui_context());
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&VCATimeAxisView::parameter_changed, this, _1), gui_context());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &VCATimeAxisView::parameter_changed));
}

VCATimeAxisView::~VCATimeAxisView ()
{
	delete automation_action_menu;
	CatchDeletion (this);
}

void
VCATimeAxisView::self_delete ()
{
	/* reset reference immediately rather than deferring to idle */
	_vca.reset ();
	delete_when_idle (this);
}

void
VCATimeAxisView::parameter_changed (std::string const & p)
{
	if (p == "track-name-number") {
		update_track_number_visibility();
	} else if (p == "solo-control-is-listen-control" || p == "listen-position") {
		set_button_names ();
	}
}

bool
VCATimeAxisView::solo_release (GdkEventButton*)
{
	/* We use NoGroup because VCA controls are never part of a group. This
	   is redundant, but clear.
	*/
	_vca->solo_control()->set_value (_vca->solo_control()->self_soloed() ? 0.0 : 1.0, Controllable::NoGroup);
	return true;
}

bool
VCATimeAxisView::mute_release (GdkEventButton*)
{
	/* We use NoGroup because VCA controls are never part of a group. This
	   is redundant, but clear.
	*/
	_vca->mute_control()->set_value (_vca->mute_control()->muted_by_self() ? 0.0 : 1.0, Controllable::NoGroup);
	return true;
}

void
VCATimeAxisView::set_vca (boost::shared_ptr<VCA> v)
{
	StripableTimeAxisView::set_stripable (v);
	_vca = v;

	gain_meter.set_controls (boost::shared_ptr<Route>(),
	                         boost::shared_ptr<PeakMeter>(),
	                         boost::shared_ptr<Amp>(),
	                         _vca->gain_control());

	// Mixer_UI::instance()->show_vca_change.connect (sigc::mem_fun (*this, &VCAMasterStrip::spill_change));

	_vca->PropertyChanged.connect (vca_connections, invalidator (*this), boost::bind (&VCATimeAxisView::vca_property_changed, this, _1), gui_context());

	_vca->solo_control()->Changed.connect (vca_connections, invalidator (*this), boost::bind (&VCATimeAxisView::update_solo_display, this), gui_context());
	_vca->mute_control()->Changed.connect (vca_connections, invalidator (*this), boost::bind (&VCATimeAxisView::update_mute_display, this), gui_context());
	_vca->DropReferences.connect (vca_connections, invalidator (*this), boost::bind (&VCATimeAxisView::self_delete, this), gui_context());

	solo_button.set_controllable (_vca->solo_control());
	mute_button.set_controllable (_vca->mute_control());

	/* VCA number never changes */
	number_label.set_text (PBD::to_string (_vca->number()));

	set_height (preset_height (HeightNormal));

	if (automation_child (GainAutomation) == 0) {
		create_automation_child (GainAutomation, false);
	}
	if (automation_child (MuteAutomation) == 0) {
		create_automation_child (MuteAutomation, false);
	}

	update_vca_name ();
	set_button_names ();
	update_solo_display ();
	update_mute_display ();
	update_track_number_visibility ();
}

void
VCATimeAxisView::vca_property_changed (PropertyChange const & what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		update_vca_name ();
	}
}

void
VCATimeAxisView::update_vca_name ()
{
	name_label.set_text (_vca->full_name());
}

bool
VCATimeAxisView::name_entry_changed (std::string const& str)
{
	_vca->set_name (str);
	return true;
}

void
VCATimeAxisView::update_mute_display ()
{
	if (_vca->mute_control()->muted_by_self()) {
		mute_button.set_active_state (ExplicitActive);
	} else if (_vca->mute_control()->muted_by_masters ()) {
		mute_button.set_active_state (ImplicitActive);
	} else {
		mute_button.set_active_state (Gtkmm2ext::Off);
	}
}

void
VCATimeAxisView::update_solo_display ()
{
	if (_vca->solo_control()->self_soloed()) {
		solo_button.set_active_state (ExplicitActive);
	} else if (_vca->solo_control()->soloed_by_masters ()) {
		solo_button.set_active_state (ImplicitActive);
	} else {
		solo_button.set_active_state (Gtkmm2ext::Off);
	}

	update_mute_display ();
}

std::string
VCATimeAxisView::name() const
{
	return _vca->name();
}

std::string
VCATimeAxisView::state_id() const
{
	return string_compose ("vtv %1", _vca->id().to_s());
}

void
VCATimeAxisView::set_button_names ()
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
VCATimeAxisView::update_track_number_visibility ()
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
VCATimeAxisView::automation_click (GdkEventButton* ev)
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
VCATimeAxisView::drop_release (GdkEventButton*)
{
	_vca->Drop (); /* EMIT SIGNAL */

	return true;
}

PresentationInfo const &
VCATimeAxisView::presentation_info () const
{
	return _vca->presentation_info();
}

boost::shared_ptr<Stripable>
VCATimeAxisView::stripable () const
{
	return _vca;
}

Gdk::Color
VCATimeAxisView::color () const
{
	return ARDOUR_UI_UTILS::gdk_color_from_rgb (_vca->presentation_info().color ());
}

void
VCATimeAxisView::set_height (uint32_t h, TrackHeightMode m)
{
	TimeAxisView::set_height (h, m);
	if (height >= preset_height (HeightNormal)) {
		drop_button.show ();
		automation_button.show ();
		gain_meter.get_gain_slider().show ();
	} else {
		drop_button.hide ();
		automation_button.hide ();
		gain_meter.get_gain_slider().hide ();
	}

	set_gui_property ("height", h);
	_vca->gui_changed ("track_height", (void*) 0); /* EMIT SIGNAL */
}

bool
VCATimeAxisView::marked_for_display () const
{
	return _vca && !_vca->presentation_info().hidden();
}

bool
VCATimeAxisView::set_marked_for_display (bool yn)
{
	if (_vca && (yn == _vca->presentation_info().hidden())) {
		_vca->presentation_info().set_hidden (!yn);
		return true; // things changed
	}
	return false;
}

void
VCATimeAxisView::create_gain_automation_child (const Evoral::Parameter& param, bool show)
{
	boost::shared_ptr<AutomationControl> c = _vca->gain_control();
	if (!c) {
		error << "VCA has no gain automation, unable to add automation track view." << endmsg;
		return;
	}

	gain_track.reset (new AutomationTimeAxisView (_session,
						      _vca, boost::shared_ptr<Automatable> (), c, param,
						      _editor,
						      *this,
						      false,
						      parent_canvas,
						      /*_route->amp()->describe_parameter(param)*/"Fader"));

	add_automation_child (Evoral::Parameter(GainAutomation), gain_track, show);
}

void
VCATimeAxisView::create_mute_automation_child (const Evoral::Parameter& param, bool show)
{
	boost::shared_ptr<AutomationControl> c = _vca->mute_control();
	if (!c) {
		error << "VCA has no mute automation, unable to add automation track view." << endmsg;
		return;
	}

	mute_track.reset (new AutomationTimeAxisView (_session,
						      _vca, boost::shared_ptr<Automatable> (), c, param,
						      _editor,
						      *this,
						      false,
						      parent_canvas,
						      /*_route->describe_parameter(param)*/ "Mute"));

	add_automation_child (Evoral::Parameter(MuteAutomation), mute_track, show);
}

void
VCATimeAxisView::create_automation_child (const Evoral::Parameter& param, bool show)
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
VCATimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;
	/* prepare it */
	TimeAxisView::build_display_menu ();

	MenuList& items = display_menu->items();
	items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &VCATimeAxisView::choose_color)));

	build_size_menu ();
	items.push_back (MenuElem (_("Height"), *_size_menu));
	items.push_back (SeparatorElem());

	build_automation_action_menu (true);
	items.push_back (MenuElem (_("Automation"), *automation_action_menu));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Drop All Slaves"), sigc::mem_fun (*this, &VCATimeAxisView::drop_all_slaves)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(_editor, &PublicEditor::remove_tracks)));
}


void
VCATimeAxisView::build_automation_action_menu (bool for_selection)
{
	using namespace Menu_Helpers;
	_main_automation_menu_map.clear ();
	delete automation_action_menu;
	automation_action_menu = new Menu;

	MenuList& items = automation_action_menu->items();

	automation_action_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Show All Automation"),
	                           sigc::bind (sigc::mem_fun (*this, &VCATimeAxisView::show_all_automation), for_selection)));

	items.push_back (MenuElem (_("Show Existing Automation"),
	                           sigc::bind (sigc::mem_fun (*this, &VCATimeAxisView::show_existing_automation), for_selection)));

	items.push_back (MenuElem (_("Hide All Automation"),
	                           sigc::bind (sigc::mem_fun (*this, &VCATimeAxisView::hide_all_automation), for_selection)));

	if (gain_track) {
		items.push_back (CheckMenuElem (_("Fader"), sigc::mem_fun (*this, &VCATimeAxisView::update_gain_track_visibility)));
		gain_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		gain_automation_item->set_active (string_to<bool>(gain_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(GainAutomation)] = gain_automation_item;
	}

	if (trim_track) {
		items.push_back (CheckMenuElem (_("Trim"), sigc::mem_fun (*this, &VCATimeAxisView::update_trim_track_visibility)));
		trim_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		trim_automation_item->set_active (string_to<bool>(trim_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(TrimAutomation)] = trim_automation_item;
	}

	if (mute_track) {
		items.push_back (CheckMenuElem (_("Mute"), sigc::mem_fun (*this, &VCATimeAxisView::update_mute_track_visibility)));
		mute_automation_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		mute_automation_item->set_active (string_to<bool>(mute_track->gui_property ("visible")));

		_main_automation_menu_map[Evoral::Parameter(MuteAutomation)] = mute_automation_item;
	}
}

void
VCATimeAxisView::show_all_automation (bool apply_to_selection)
{
	assert (!apply_to_selection); // VCAs can't yet be selected
	no_redraw = true;

	StripableTimeAxisView::show_all_automation ();

	no_redraw = false;
	request_redraw ();
}

void
VCATimeAxisView::show_existing_automation (bool apply_to_selection)
{
	assert (!apply_to_selection); // VCAs can't yet be selected
	no_redraw = true;

	StripableTimeAxisView::show_existing_automation ();

	no_redraw = false;
	request_redraw ();
}

void
VCATimeAxisView::hide_all_automation (bool apply_to_selection)
{
	assert (!apply_to_selection); // VCAs can't yet be selected
	no_redraw = true;

	StripableTimeAxisView::hide_all_automation ();

	no_redraw = false;
	request_redraw ();
}

void
VCATimeAxisView::drop_all_slaves ()
{
	_vca->Drop (); /* EMIT SIGNAL */

	if (Mixer_UI::instance()->showing_spill_for (_vca)) {
		Mixer_UI::instance()->show_spill (boost::shared_ptr<Stripable>());
	}
}

void
VCATimeAxisView::choose_color () {
	_color_picker.popup (_vca);
}
