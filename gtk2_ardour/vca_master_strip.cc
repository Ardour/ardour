/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <gtkmm/stock.h>

#include "pbd/string_convert.h"

#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/keyboard.h"
#include "widgets/tooltips.h"

#include "ardour_dialog.h"
#include "ardour_message.h"
#include "floating_text_entry.h"
#include "gui_thread.h"
#include "mixer_ui.h"
#include "ui_config.h"
#include "utils.h"
#include "vca_master_strip.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace PBD;
using std::string;

PBD::Signal1<void,VCAMasterStrip*> VCAMasterStrip::CatchDeletion;

VCAMasterStrip::VCAMasterStrip (Session* s, boost::shared_ptr<VCA> v)
	: SessionHandlePtr (s)
	, _vca (v)
	, gain_meter (s, 254) /* magic number, don't adjust blindly */
	, context_menu (0)
	, delete_dialog (0)
	, control_slave_ui (s)
{
	/* set color for the VCA, if not already done. */

	if (!_vca->presentation_info().color_set()) {
		_vca->presentation_info().set_color (ARDOUR_UI_UTILS::gdk_color_to_rgba (unique_random_color()));
	}

	control_slave_ui.set_stripable (boost::dynamic_pointer_cast<Stripable> (v));

	gain_meter.set_controls (boost::shared_ptr<Route>(),
	                         boost::shared_ptr<PeakMeter>(),
	                         boost::shared_ptr<Amp>(),
	                         _vca->gain_control());

	solo_button.set_name ("solo button");
	set_tooltip (solo_button, _("Solo slaves"));
	solo_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCAMasterStrip::solo_release), false);

	mute_button.set_name ("mute button");
	mute_button.set_text (_("M"));
	set_tooltip (mute_button, _("Mute slaves"));
	mute_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCAMasterStrip::mute_release), false);

	hide_button.set_icon (ArdourIcon::HideEye);
	set_tooltip (&hide_button, _("Hide this VCA strip"));

	hide_button.signal_clicked.connect (sigc::mem_fun(*this, &VCAMasterStrip::hide_clicked));

	solo_mute_box.set_spacing (2);
	solo_mute_box.pack_start (mute_button, true, true);
	solo_mute_box.pack_start (solo_button, true, true);

	mute_button.set_controllable (_vca->mute_control());
	solo_button.set_controllable (_vca->solo_control());

	number_label.set_text (PBD::to_string (v->number()));
	number_label.set_elements((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text|ArdourButton::Inactive));
	number_label.set_no_show_all ();
	number_label.set_name ("generic button");
	number_label.set_alignment (.5, .5);
	number_label.set_fallthrough_to_parent (true);
	number_label.set_inactive_color (_vca->presentation_info().color ());
	number_label.signal_button_release_event().connect (sigc::mem_fun (*this, &VCAMasterStrip::number_button_press), false);

	update_bottom_padding ();

	//Glib::RefPtr<Pango::Layout> layout = vertical_button.get_layout ();
	// layout->set_justify (JUSTIFY_CENTER);
	/* horizontally centered, with a little space (5%) at the top */
	vertical_button.set_angle (90);
	vertical_button.set_layout_font (UIConfiguration::instance().get_NormalBoldFont());
	vertical_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCAMasterStrip::vertical_button_press));
	vertical_button.set_fallthrough_to_parent (true);
	vertical_button.set_active_color (_vca->presentation_info().color ());
	set_tooltip (vertical_button, _("Click to show slaves only")); /* tooltip updated dynamically */

	global_vpacker.set_border_width (0);
	global_vpacker.set_spacing (0);
	gain_meter.set_spacing(4);

	global_vpacker.pack_start (number_label, false, false, 1);
	global_vpacker.pack_start (hide_button, false, false, 1);
	global_vpacker.pack_start (vertical_button, true, true, 1);
	global_vpacker.pack_start (solo_mute_box, false, false, 1);
	global_vpacker.pack_start (gain_meter, false, false, 1);
	global_vpacker.pack_start (control_slave_ui, false, false, 1);
	global_vpacker.pack_start (gain_meter.gain_automation_state_button, false, false, 1);
	global_vpacker.pack_start (bottom_padding, false, false, 0);

	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	global_vpacker.show ();
	global_frame.show ();
	vertical_button.show ();
	hide_button.show ();
	number_label.show ();
	gain_meter.show ();
	solo_mute_box.show_all ();
	control_slave_ui.show ();
	gain_meter.gain_automation_state_button.show ();

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);
	set_solo_text ();
	update_vca_name ();
	solo_changed ();
	mute_changed ();
	spill_change (boost::shared_ptr<VCA>());

	Mixer_UI::instance()->show_spill_change.connect (sigc::mem_fun (*this, &VCAMasterStrip::spill_change));

	_vca->PropertyChanged.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::vca_property_changed, this, _1), gui_context());
	_vca->presentation_info().PropertyChanged.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::vca_property_changed, this, _1), gui_context());
	_vca->DropReferences.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::self_delete, this), gui_context());

	_vca->solo_control()->Changed.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::solo_changed, this), gui_context());
	_vca->mute_control()->Changed.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::mute_changed, this), gui_context());

	_session->MonitorBusAddedOrRemoved.connect (*this, invalidator (*this), boost::bind (&VCAMasterStrip::set_button_names, this), gui_context());

	s->config.ParameterChanged.connect (*this, invalidator (*this), boost::bind (&VCAMasterStrip::parameter_changed, this, _1), gui_context());
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&VCAMasterStrip::parameter_changed, this, _1), gui_context());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &VCAMasterStrip::parameter_changed));
}

VCAMasterStrip::~VCAMasterStrip ()
{
	if ((_session && !_session->deletion_in_progress()) && Mixer_UI::instance()->showing_spill_for (_vca)) {
		/* cancel spill for this VCA */
		Mixer_UI::instance()->show_spill (boost::shared_ptr<Stripable>());
	}

	delete delete_dialog;
	delete context_menu;

	CatchDeletion (this); /* EMIT SIGNAL */
}

void
VCAMasterStrip::self_delete ()
{
	if ((_session && !_session->deletion_in_progress()) && Mixer_UI::instance()->showing_spill_for (_vca)) {
		/* cancel spill for this VCA */
		Mixer_UI::instance()->show_spill (boost::shared_ptr<Stripable>());
	}
	/* Drop reference immediately, delete self when idle */
	_vca.reset ();
	gain_meter.set_controls (boost::shared_ptr<Route>(),
	                         boost::shared_ptr<PeakMeter>(),
	                         boost::shared_ptr<Amp>(),
	                         boost::shared_ptr<GainControl>());
	delete_when_idle (this);
}

void
VCAMasterStrip::parameter_changed (std::string const & p)
{
	if (p == "solo-control-is-listen-control" || p == "listen-position") {
		set_button_names ();
	} else if (p == "mixer-element-visibility") {
		update_bottom_padding ();
	}
}

void
VCAMasterStrip::set_button_names ()
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
VCAMasterStrip::update_bottom_padding ()
{
	std::string viz = UIConfiguration::instance().get_mixer_strip_visibility ();

	ArdourButton output_button (_("Output"));
	ArdourButton comment_button (_("Comments"));

	output_button.set_name ("mixer strip button");
	comment_button.set_name ("generic button");

	if (viz.find ("VCA") == std::string::npos) {
		control_slave_ui.hide ();
	} else {
		control_slave_ui.show ();
	}

	int h = 0;
	if (viz.find ("Output") != std::string::npos) {
		Gtk::Window window (WINDOW_TOPLEVEL);
		window.add (output_button);
		Gtk::Requisition requisition(output_button.size_request ());
		h += requisition.height + 2;
	}
	if (viz.find ("Comments") != std::string::npos) {
		Gtk::Window window (WINDOW_TOPLEVEL);
		window.add (comment_button);
		Gtk::Requisition requisition(comment_button.size_request ());
		h += requisition.height + 2;
	}
	if (h <= 0) {
		bottom_padding.set_size_request (-1, 1);
		bottom_padding.hide ();
	} else {
		bottom_padding.set_size_request (-1, h);
		bottom_padding.show ();
	}
}

string
VCAMasterStrip::name() const
{
	return _vca->name();
}

void
VCAMasterStrip::hide_clicked ()
{
	_vca->presentation_info().set_hidden (true);
}

void
VCAMasterStrip::hide_confirmation (int response)
{
	delete_dialog->hide ();

	switch (response) {
	case RESPONSE_YES:
		/* get everything to deassign. This will also delete ourselves (when
		 * idle) and that in turn will remove us from the Mixer GUI
		 */
		_session->vca_manager().remove_vca (_vca);
		break;
	default:
		break;
	}
}

bool
VCAMasterStrip::width_button_pressed (GdkEventButton* ev)
{
	return false;
}

void
VCAMasterStrip::set_selected (bool yn)
{
	AxisView::set_selected (yn);

	if (_selected) {
		global_frame.set_shadow_type (Gtk::SHADOW_ETCHED_OUT);
		global_frame.set_name ("MixerStripSelectedFrame");
	} else {
		global_frame.set_shadow_type (Gtk::SHADOW_IN);
		global_frame.set_name ("MixerStripFrame");
	}

	global_frame.queue_draw ();
}

bool
VCAMasterStrip::solo_release (GdkEventButton*)
{
	/* We use NoGroup because VCA controls are never part of a group. This
	   is redundant, but clear.
	*/
	_vca->solo_control()->set_value (_vca->solo_control()->self_soloed() ? 0.0 : 1.0, Controllable::NoGroup);
	return true;
}

bool
VCAMasterStrip::mute_release (GdkEventButton*)
{
	/* We use NoGroup because VCA controls are never part of a group. This
	   is redundant, but clear.
	*/
	_vca->mute_control()->set_value (_vca->mute_control()->muted_by_self() ? 0.0 : 1.0, Controllable::NoGroup);
	return true;
}

void
VCAMasterStrip::set_solo_text ()
{
	if (Config->get_solo_control_is_listen_control ()) {
		switch (Config->get_listen_position()) {
		case AfterFaderListen:
			solo_button.set_text (_("A"));
			break;
		case PreFaderListen:
			solo_button.set_text (_("P"));
			break;
			}
	} else {
		solo_button.set_text (_("S"));
	}
}

void
VCAMasterStrip::mute_changed ()
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
VCAMasterStrip::solo_changed ()
{
	if (_vca->solo_control()->self_soloed()) {
		solo_button.set_active_state (ExplicitActive);
	} else if (_vca->solo_control()->soloed_by_masters ()) {
		solo_button.set_active_state (ImplicitActive);
	} else {
		solo_button.set_active_state (Gtkmm2ext::Off);
	}
}

bool
VCAMasterStrip::vertical_button_press (GdkEventButton* ev)
{
	if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {
		start_name_edit ();
		return true;
	}

	if (Keyboard::is_context_menu_event (ev)) {
		if (!context_menu) {
			build_context_menu ();
		}
		context_menu->popup (1, ev->time);
		return true;
	}

	if (ev->button == 1) {
		spill ();
	}

	return true;
}

bool
VCAMasterStrip::number_button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		if (!context_menu) {
			build_context_menu ();
		}
		context_menu->popup (1, ev->time);
		return true;
	}
	return false;
}

void
VCAMasterStrip::start_name_edit ()
{
	Gtk::Window* win = dynamic_cast<Gtk::Window*>(get_toplevel());
	FloatingTextEntry* fte = new FloatingTextEntry (win, _vca->name());
	fte->use_text.connect (sigc::mem_fun (*this, &VCAMasterStrip::finish_name_edit));
	fte->present ();
}

void
VCAMasterStrip::finish_name_edit (std::string str, int)
{
	_vca->set_name (str);
}

void
VCAMasterStrip::vca_property_changed (PropertyChange const & what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		update_vca_name ();
	}

	if (what_changed.contains (ARDOUR::Properties::color)) {
		vertical_button.set_active_color (_vca->presentation_info().color ());
		number_label.set_inactive_color (_vca->presentation_info().color ());
	}

	if (what_changed.contains (ARDOUR::Properties::hidden)) {

	}
}

void
VCAMasterStrip::update_vca_name ()
{
	/* 20 is a rough guess at the number of letters we can fit. */
	vertical_button.set_text (short_version (_vca->full_name(), 20));
}

void
VCAMasterStrip::build_context_menu ()
{
	using namespace Gtk::Menu_Helpers;
	context_menu = new Menu;
	MenuList& items = context_menu->items();
	items.push_back (MenuElem (_("Rename"), sigc::mem_fun (*this, &VCAMasterStrip::start_name_edit)));
	items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &VCAMasterStrip::start_color_edit)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Assign Selected Channels"), sigc::mem_fun (*this, &VCAMasterStrip::assign_all_selected)));
	items.push_back (MenuElem (_("Drop Selected Channels"), sigc::mem_fun (*this, &VCAMasterStrip::unassign_all_selected)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Drop All Slaves"), sigc::mem_fun (*this, &VCAMasterStrip::drop_all_slaves)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun (*this, &VCAMasterStrip::remove)));
}

void
VCAMasterStrip::spill ()
{
	if (Mixer_UI::instance()->showing_spill_for (_vca)) {
		Mixer_UI::instance()->show_spill (boost::shared_ptr<Stripable>());
	} else {
		Mixer_UI::instance()->show_spill (_vca);
	}
}

void
VCAMasterStrip::spill_change (boost::shared_ptr<Stripable> vca)
{
	if (vca != _vca) {
		vertical_button.set_active_state (Gtkmm2ext::Off);
		set_tooltip (vertical_button, _("Click to show slaves only"));
	} else {
		vertical_button.set_active_state (Gtkmm2ext::ExplicitActive);
		set_tooltip (vertical_button, _("Click to show normal mixer"));
	}
}

void
VCAMasterStrip::remove ()
{
	if (!_session) {
		return;
	}

	ArdourMessageDialog checker (_("Do you really want to remove this VCA?"),
	                             true,
	                             Gtk::MESSAGE_QUESTION,
	                             Gtk::BUTTONS_NONE);

	string title = string_compose (_("Remove %1"), "VCA");
	checker.set_title (title);

	checker.set_secondary_text(_("This action cannot be undone."));

	checker.add_button (_("No, do nothing."), RESPONSE_CANCEL);
	checker.add_button (_("Yes, remove it."), RESPONSE_ACCEPT);
	checker.set_default_response (RESPONSE_CANCEL);

	checker.set_name (X_("RemoveVcaDialog"));
	checker.set_wmclass (X_("ardour_vca_remove"), PROGRAM_NAME);
	checker.set_position (Gtk::WIN_POS_MOUSE);

	switch (checker.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}
	checker.hide();

	_session->vca_manager().remove_vca (_vca);
}

void
VCAMasterStrip::assign_all_selected ()
{
	Mixer_UI::instance()->do_vca_assign (_vca);
}

void
VCAMasterStrip::unassign_all_selected ()
{
	Mixer_UI::instance()->do_vca_unassign (_vca);
}

void
VCAMasterStrip::drop_all_slaves ()
{
	_vca->Drop (); /* EMIT SIGNAL */

	if (Mixer_UI::instance()->showing_spill_for (_vca)) {
		Mixer_UI::instance()->show_spill (boost::shared_ptr<Stripable>());
	}
}

Gdk::Color
VCAMasterStrip::color () const
{
	return ARDOUR_UI_UTILS::gdk_color_from_rgba (_vca->presentation_info().color ());
}

string
VCAMasterStrip::state_id () const
{
	return string_compose (X_("vms-%1"), _vca->number());
}

void
VCAMasterStrip::start_color_edit ()
{
	_color_picker.popup (_vca);
}

bool
VCAMasterStrip::marked_for_display () const
{
	return !_vca->presentation_info().hidden();
}

bool
VCAMasterStrip::set_marked_for_display (bool yn)
{
	if (yn == _vca->presentation_info().hidden()) {
		_vca->presentation_info().set_hidden (!yn);
		return true; // things changed
	}
	return false;
}

PresentationInfo const &
VCAMasterStrip::presentation_info () const
{
	return _vca->presentation_info();
}

boost::shared_ptr<Stripable>
VCAMasterStrip::stripable () const
{
	return _vca;
}
