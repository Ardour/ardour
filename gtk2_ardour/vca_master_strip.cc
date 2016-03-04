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

#include "pbd/convert.h"

#include "ardour/rc_configuration.h"
#include "ardour/vca.h"

#include "tooltips.h"
#include "vca_master_strip.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace Gtkmm2ext;
using std::string;

VCAMasterStrip::VCAMasterStrip (Session* s, boost::shared_ptr<VCA> v)
	: AxisView (s)
	, _vca (v)
	, gain_meter (s, 250)
	, wide (true)
{
	gain_meter.set_controls (boost::shared_ptr<Route>(),
	                         boost::shared_ptr<PeakMeter>(),
	                         boost::shared_ptr<Amp>(),
	                         _vca->control());

	solo_button.set_name ("solo button");
	set_tooltip (solo_button, _("Solo slaves"));
	solo_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCAMasterStrip::solo_release), false);

	mute_button.set_name ("mute button");
	set_tooltip (mute_button, _("Mute slaves"));
	mute_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VCAMasterStrip::mute_release), false);

	hide_button.set_icon (ArdourIcon::CloseCross);
	set_tooltip (&hide_button, _("Hide this VCA strip"));

	width_button.set_icon (ArdourIcon::StripWidth);
	set_tooltip (width_button, _("Click to toggle the width of this VCA strip."));

	assign_button.set_text (_("-vca-"));
	set_tooltip (assign_button, _("Click to assign a VCA Master to this VCA"));

	width_button.signal_button_press_event().connect (sigc::mem_fun(*this, &VCAMasterStrip::width_button_pressed), false);
	hide_button.signal_clicked.connect (sigc::mem_fun(*this, &VCAMasterStrip::hide_clicked));

	width_hide_box.set_spacing (2);
	width_hide_box.pack_start (width_button, false, true);
	width_hide_box.pack_start (number_label, true, true);
	width_hide_box.pack_end (hide_button, false, true);

	solo_mute_box.set_spacing (2);
	solo_mute_box.pack_start (mute_button, true, true);
	solo_mute_box.pack_start (solo_button, true, true);

	number_label.set_text (PBD::to_string (v->number(), std::dec));
	number_label.set_elements((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text|ArdourButton::Inactive));
	number_label.set_no_show_all ();
	number_label.set_name ("tracknumber label");
	number_label.set_fixed_colors (0x80808080, 0x80808080);
	number_label.set_alignment (.5, .5);
	number_label.set_fallthrough_to_parent (true);

	name_button.set_text (_vca->name());
	active_button.set_text ("active");

	top_padding.set_size_request (-1, 16); /* must match height in GroupTabs::set_size_request() */
	bottom_padding.set_size_request (-1, 50); /* this one is a hack. there's no trivial way to compute it */

	global_vpacker.set_border_width (1);
	global_vpacker.set_spacing (0);

	global_vpacker.pack_start (top_padding, false, false);
	global_vpacker.pack_start (width_hide_box, false, false);
	global_vpacker.pack_start (active_button, false, false);
	global_vpacker.pack_start (name_button, false, false);
	global_vpacker.pack_start (vertical_padding, true, true);
	global_vpacker.pack_start (solo_mute_box, false, false);
	global_vpacker.pack_start (gain_meter, false, false);
	global_vpacker.pack_start (assign_button, false, false);
	global_vpacker.pack_start (bottom_padding, false, false);

	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	global_vpacker.show ();
	global_frame.show ();
	top_padding.show ();
	bottom_padding.show ();
	vertical_padding.show ();
	width_hide_box.show_all ();
	active_button.show_all ();
	name_button.show_all ();
	gain_meter.show_all ();
	solo_mute_box.show_all ();
	assign_button.show ();

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);
	set_width (true);

	_vca->SoloChange.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::solo_changed, this), gui_context());
	_vca->MuteChange.connect (vca_connections, invalidator (*this), boost::bind (&VCAMasterStrip::mute_changed, this), gui_context());
}

string
VCAMasterStrip::name() const
{
	return _vca->name();
}

void
VCAMasterStrip::hide_clicked ()
{
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
	_vca->set_solo (!_vca->soloed());
	return true;
}

bool
VCAMasterStrip::mute_release (GdkEventButton*)
{
	_vca->set_mute (!_vca->muted());
	return true;
}

void
VCAMasterStrip::set_solo_text ()
{
	if (wide) {
		if (Config->get_solo_control_is_listen_control ()) {
			switch (Config->get_listen_position()) {
			case AfterFaderListen:
				solo_button.set_text (_("AFL"));
				break;
			case PreFaderListen:
				solo_button.set_text (_("PFL"));
				break;
			}
		} else {
			solo_button.set_text (_("Solo"));
		}
	} else {
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
}

void
VCAMasterStrip::set_width (bool w)
{
	wide = w;

	if (wide) {
		mute_button.set_text (_("Mute"));
	} else {
		mute_button.set_text (_("m"));
	}

	set_solo_text ();
}

void
VCAMasterStrip::mute_changed ()
{
	if (_vca->muted()) {
		mute_button.set_active_state (ExplicitActive);
	} else {
		mute_button.set_active_state (Gtkmm2ext::Off);
	}
}

void
VCAMasterStrip::solo_changed ()
{
	if (_vca->soloed()) {
		solo_button.set_active_state (ExplicitActive);
	} else {
		solo_button.set_active_state (Gtkmm2ext::Off);
	}
}
