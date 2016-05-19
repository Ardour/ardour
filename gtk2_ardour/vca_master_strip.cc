/*
    Copyright (C) 2016 Paul Davis

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

#include <gtkmm/stock.h>

#include "pbd/convert.h"

#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/keyboard.h"

#include "ardour_dialog.h"
#include "floating_text_entry.h"
#include "gui_thread.h"
#include "mixer_ui.h"
#include "tooltips.h"
#include "ui_config.h"
#include "vca_master_strip.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace PBD;
using std::string;

PBD::Signal1<void,VCAMasterStrip*> VCAMasterStrip::CatchDeletion;

VCAMasterStrip::VCAMasterStrip (Session* s, boost::shared_ptr<VCA> v)
	: AxisView (s)
	, _vca (v)
	, gain_meter (s, 250)
	, context_menu (0)
	, delete_dialog (0)
{
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

	hide_button.set_icon (ArdourIcon::CloseCross);
	set_tooltip (&hide_button, _("Hide this VCA strip"));

	assign_button.set_name (X_("vca assign"));
	set_tooltip (assign_button, _("Click to assign a VCA Master to this VCA"));
	assign_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCAMasterStrip::vca_button_release), false);

	hide_button.signal_clicked.connect (sigc::mem_fun(*this, &VCAMasterStrip::hide_clicked));

	width_hide_box.pack_start (number_label, true, true);
	width_hide_box.pack_end (hide_button, false, true);

	solo_mute_box.set_spacing (2);
	solo_mute_box.pack_start (mute_button, true, true);
	solo_mute_box.pack_start (solo_button, true, true);

	number_label.set_text (to_string (v->number(), std::dec));
	number_label.set_elements((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text|ArdourButton::Inactive));
	number_label.set_no_show_all ();
	number_label.set_name ("generic button");
	number_label.set_alignment (.5, .5);
	number_label.set_fallthrough_to_parent (true);

	top_padding.set_size_request (-1, 16); /* must match height in GroupTabs::set_size_request() */
	bottom_padding.set_size_request (-1, 30); /* this one is a hack. there's no trivial way to compute it */

	//Glib::RefPtr<Pango::Layout> layout = vertical_button.get_layout ();
	// layout->set_justify (JUSTIFY_CENTER);
	/* horizontally centered, with a little space (5%) at the top */
	vertical_button.set_angle (90);
	vertical_button.set_layout_font (UIConfiguration::instance().get_NormalBoldFont());
	vertical_button.signal_clicked.connect (sigc::mem_fun (*this, &VCAMasterStrip::spill));

	drop_button.set_text(_("drop"));
	drop_button.signal_clicked.connect (sigc::mem_fun (*this, &VCAMasterStrip::drop_button_press));
	set_tooltip (drop_button, _("Unassign all slaves from this control master"));

	global_vpacker.set_border_width (1);
	global_vpacker.set_spacing (0);

	global_vpacker.pack_start (top_padding, false, false);
	global_vpacker.pack_start (width_hide_box, false, false);
	global_vpacker.pack_start (vertical_button, true, true);
	global_vpacker.pack_start (solo_mute_box, false, false);
	global_vpacker.pack_start (gain_meter, false, false);
	global_vpacker.pack_start (assign_button, false, false);
	global_vpacker.pack_start (drop_button, false, false);
	global_vpacker.pack_start (bottom_padding, false, false);

	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	global_vpacker.show ();
	global_frame.show ();
	top_padding.show ();
	bottom_padding.show ();
	vertical_button.show ();
	hide_button.show ();
	number_label.show ();
	width_hide_box.show ();
	gain_meter.show ();
	solo_mute_box.show_all ();
	assign_button.show ();
	drop_button.show ();

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);
	set_solo_text ();
	update_vca_display ();
	update_vca_name ();
	solo_changed ();
	mute_changed ();

	Mixer_UI::instance()->show_vca_change.connect (sigc::mem_fun (*this, &VCAMasterStrip::spill_change));

	_vca->PropertyChanged.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::vca_property_changed, this, _1), gui_context());

	_vca->solo_control()->Changed.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::solo_changed, this), gui_context());
	_vca->mute_control()->Changed.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::mute_changed, this), gui_context());

	/* only need to connect to one of these to update VCA status */

	_vca->gain_control()->MasterStatusChange.connect (vca_connections,
	                                          invalidator (*this),
	                                          boost::bind (&VCAMasterStrip::update_vca_display, this),
	                                          gui_context());


	_vca->DropReferences.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::self_delete, this), gui_context());

}

VCAMasterStrip::~VCAMasterStrip ()
{
	if ((_session && !_session->deletion_in_progress()) && Mixer_UI::instance()->showing_vca_slaves_for (_vca)) {
		/* cancel spill for this VCA */
		Mixer_UI::instance()->show_vca_slaves (boost::shared_ptr<VCA>());
	}

	delete delete_dialog;
	delete context_menu;

	CatchDeletion (this); /* EMIT SIGNAL */
}

void
VCAMasterStrip::self_delete ()
{
	delete_when_idle (this);
}

void
VCAMasterStrip::update_vca_display ()
{
	VCAList vcas (_session->vca_manager().vcas());
	string label;

	for (VCAList::iterator v = vcas.begin(); v != vcas.end(); ++v) {
		if (_vca->slaved_to (*v)) {
			if (!label.empty()) {
				label += ' ';
			}
			label += to_string ((*v)->number(), std::dec);
		}
	}

	if (label.empty()) {
		label = _("-vca-");
		assign_button.set_active_state (Gtkmm2ext::Off);
	} else {
		assign_button.set_active_state (Gtkmm2ext::ExplicitActive);
	}

	assign_button.set_text (label);
}

string
VCAMasterStrip::name() const
{
	return _vca->name();
}

void
VCAMasterStrip::hide_clicked ()
{
	if (!delete_dialog) {
		delete_dialog = new MessageDialog (_("Removing a Master will deassign all slaves. Remove it anyway?"),
		                                   true, MESSAGE_WARNING, BUTTONS_YES_NO, true);
		delete_dialog->signal_response().connect (sigc::mem_fun (*this, &VCAMasterStrip::hide_confirmation));
	}

	delete_dialog->set_position (Gtk::WIN_POS_MOUSE);
	delete_dialog->present ();
}

void
VCAMasterStrip::hide_confirmation (int response)
{
	delete_dialog->hide ();

	switch (response) {
	case RESPONSE_OK:
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

void
VCAMasterStrip::vca_menu_toggle (CheckMenuItem* menuitem, uint32_t n)
{
	boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_number (n);

	if (!menuitem->get_active()) {
		if (!vca) {
			/* null VCA means drop all VCA assignments */
			_vca->unassign (boost::shared_ptr<VCA>());

		} else {
			_vca->unassign (vca);
		}
	} else {
		if (vca) {
			_vca->assign (vca);
		}
	}
}

void
VCAMasterStrip::unassign ()
{
	_vca->unassign (boost::shared_ptr<VCA>());
}

bool
VCAMasterStrip::vca_button_release (GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	if (!_session) {
		return false;
	}

	/* primary click only */

	if (ev->button != 1) {
		return false;
	}

	VCAList vcas (_session->vca_manager().vcas());

	if (vcas.empty()) {
		/* XXX should probably show a message saying "No VCA masters" */
		return true;
	}

	Menu* menu = new Menu;
	MenuList& items = menu->items();

	items.push_back (MenuElem (_("Unassign"), sigc::mem_fun (*this, &VCAMasterStrip::unassign)));

	for (VCAList::iterator v = vcas.begin(); v != vcas.end(); ++v) {

		if (*v == _vca) {
			/* no self-mastering */
			continue;
		}

		items.push_back (CheckMenuElem ((*v)->name()));
		CheckMenuItem* item = dynamic_cast<CheckMenuItem*> (&items.back());
		if (_vca->gain_control()->slaved_to ((*v)->gain_control())) {
			item->set_active (true);
		}
		item->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &VCAMasterStrip::vca_menu_toggle), item, (*v)->number()));
	}

	menu->popup (1, ev->time);

	return true;
}

bool
VCAMasterStrip::vertical_box_press (GdkEventButton* ev)
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
		// spill ();
	}

	return true;
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
VCAMasterStrip::finish_name_edit (std::string str)
{
	_vca->set_name (str);
}

void
VCAMasterStrip::vca_property_changed (PropertyChange const & what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		update_vca_name ();
	}
}

void
VCAMasterStrip::update_vca_name ()
{
	/* 20 is a rough guess at the number of letters we can fit. */
	vertical_button.set_text (short_version (_vca->name(), 20));
}

void
VCAMasterStrip::build_context_menu ()
{
	using namespace Gtk::Menu_Helpers;
	context_menu = new Menu;
	MenuList& items = context_menu->items();
	items.push_back (MenuElem (_("Rename"), sigc::mem_fun (*this, &VCAMasterStrip::start_name_edit)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Drop All Slaves"), sigc::mem_fun (*this, &VCAMasterStrip::drop_all_slaves)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun (*this, &VCAMasterStrip::remove)));
}

void
VCAMasterStrip::spill ()
{
	if (Mixer_UI::instance()->showing_vca_slaves_for (_vca)) {
		Mixer_UI::instance()->show_vca_slaves (boost::shared_ptr<VCA>());
	} else {
		Mixer_UI::instance()->show_vca_slaves (_vca);
	}
}

void
VCAMasterStrip::spill_change (boost::shared_ptr<VCA> vca)
{
	if (vca != _vca) {
		vertical_button.set_active_state (Gtkmm2ext::Off);
	} else {
		vertical_button.set_active_state (Gtkmm2ext::ExplicitActive);
	}
}

void
VCAMasterStrip::remove ()
{
	if (!_session) {
		return;
	}

	_session->vca_manager().remove_vca (_vca);
}

void
VCAMasterStrip::drop_all_slaves ()
{
	_vca->Drop (); /* EMIT SIGNAL */

	if (Mixer_UI::instance()->showing_vca_slaves_for (_vca)) {
		Mixer_UI::instance()->show_vca_slaves (boost::shared_ptr<VCA>());
	}
}

void
VCAMasterStrip::drop_button_press ()
{
	drop_all_slaves ();
}
