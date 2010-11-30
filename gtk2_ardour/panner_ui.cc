/*
  Copyright (C) 2004 Paul Davis

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

#include <limits.h>

#include "ardour/io.h"
#include "ardour/dB.h"
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/barcontroller.h>
#include "midi++/manager.h"
#include "pbd/fastlog.h"

#include "ardour_ui.h"
#include "panner_ui.h"
#include "panner2d.h"
#include "utils.h"
#include "panner.h"
#include "gui_thread.h"

#include "ardour/delivery.h"
#include "ardour/session.h"
#include "ardour/panner.h"
#include "ardour/route.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;

const int PannerUI::pan_bar_height = 20;
Glib::RefPtr<Gdk::Pixbuf> PannerUI::_poswidth_slider;

PannerUI::PannerUI (Session* s)
	: _current_nouts (-1)
	, _current_npans (-1)
	, hAdjustment(0.0, 0.0, 0.0)
	, vAdjustment(0.0, 0.0, 0.0)
	, panning_viewport(hAdjustment, vAdjustment)
	, panning_up_arrow (Gtk::ARROW_UP, Gtk::SHADOW_OUT)
	, panning_down_arrow (Gtk::ARROW_DOWN, Gtk::SHADOW_OUT)
        , _position_adjustment (0.5, 0.0, 1.0, 0.01, 0.1)
        , _width_adjustment (0.0, -1.0, 1.0, 0.01, 0.1)
	, panning_link_button (_("link"))
	, pan_automation_style_button ("")
	, pan_automation_state_button ("")
	, _bar_spinner_active (false)
{
	set_session (s);

	ignore_toggle = false;
	pan_menu = 0;
	pan_astate_menu = 0;
	pan_astyle_menu = 0;
	in_pan_update = false;
        _position_fader = 0;
        _width_fader = 0;
        _ignore_width_change = false;
        _ignore_position_change = false;

	pan_automation_style_button.set_name ("MixerAutomationModeButton");
	pan_automation_state_button.set_name ("MixerAutomationPlaybackButton");

	ARDOUR_UI::instance()->set_tip (pan_automation_state_button, _("Pan automation mode"));
	ARDOUR_UI::instance()->set_tip (pan_automation_style_button, _("Pan automation type"));

	//set_size_request_to_display_given_text (pan_automation_state_button, X_("O"), 2, 2);
	//set_size_request_to_display_given_text (pan_automation_style_button, X_("0"), 2, 2);

	panning_viewport.set_name (X_("BaseFrame"));

	ARDOUR_UI::instance()->set_tip (panning_link_button,
						   _("panning link control"));
	ARDOUR_UI::instance()->set_tip (panning_link_direction_button,
						   _("panning link direction"));

	pan_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	pan_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	pan_automation_style_button.signal_button_press_event().connect (sigc::mem_fun(*this, &PannerUI::pan_automation_style_button_event), false);
	pan_automation_state_button.signal_button_press_event().connect (sigc::mem_fun(*this, &PannerUI::pan_automation_state_button_event), false);

	panning_link_button.set_name (X_("PanningLinkButton"));
	panning_link_direction_button.set_name (X_("PanningLinkDirectionButton"));

	panning_link_box.pack_start (panning_link_button, true, true);
	panning_link_box.pack_start (panning_link_direction_button, true, true);
	panning_link_box.pack_start (pan_automation_state_button, true, true);

	/* the pixmap will be reset at some point, but the key thing is that
	   we need a pixmap in the button just to get started.
	*/
	panning_link_direction_button.add (*(manage (new Image (get_xpm("forwardblarrow.xpm")))));

	panning_link_direction_button.signal_clicked().connect
		(sigc::mem_fun(*this, &PannerUI::panning_link_direction_clicked));

	panning_link_button.signal_button_press_event().connect
		(sigc::mem_fun(*this, &PannerUI::panning_link_button_press), false);
	panning_link_button.signal_button_release_event().connect
		(sigc::mem_fun(*this, &PannerUI::panning_link_button_release), false);

	panning_up.set_border_width (3);
	panning_down.set_border_width (3);
	panning_up.add (panning_up_arrow);
	panning_down.add (panning_down_arrow);
	panning_up.set_name (X_("PanScrollerBase"));
	panning_down.set_name (X_("PanScrollerBase"));
	panning_up_arrow.set_name (X_("PanScrollerArrow"));
	panning_down_arrow.set_name (X_("PanScrollerArrow"));

	pan_vbox.set_spacing (2);
	pan_vbox.pack_start (panning_viewport, Gtk::PACK_SHRINK);
	pan_vbox.pack_start (panning_link_box, Gtk::PACK_SHRINK);
        
	pack_start (pan_vbox, true, true);

	twod_panner = 0;
	big_window = 0;

	set_width(Narrow);
}

void
PannerUI::set_panner (boost::shared_ptr<Panner> p)
{
 	connections.drop_connections ();

	delete pan_astyle_menu;
	pan_astyle_menu = 0;

	delete pan_astate_menu;
	pan_astate_menu = 0;

	_panner = p;

	delete twod_panner;
	twod_panner = 0;

	if (!_panner) {
		return;
	}

	_panner->Changed.connect (connections, invalidator (*this), boost::bind (&PannerUI::panner_changed, this, this), gui_context());
	_panner->LinkStateChanged.connect (connections, invalidator (*this), boost::bind (&PannerUI::update_pan_linkage, this), gui_context());
	_panner->StateChanged.connect (connections, invalidator (*this), boost::bind (&PannerUI::update_pan_state, this), gui_context());

	panner_changed (0);
	update_pan_sensitive ();
	update_pan_linkage ();
	pan_automation_state_changed ();
}

void
PannerUI::build_astate_menu ()
{
	using namespace Menu_Helpers;

	if (pan_astate_menu == 0) {
		pan_astate_menu = new Menu;
		pan_astate_menu->set_name ("ArdourContextMenu");
	} else {
		pan_astate_menu->items().clear ();
	}

	pan_astate_menu->items().push_back (MenuElem (_("Manual"), sigc::bind (
			sigc::mem_fun (_panner.get(), &Panner::set_automation_state),
			(AutoState) Off)));
	pan_astate_menu->items().push_back (MenuElem (_("Play"), sigc::bind (
			sigc::mem_fun (_panner.get(), &Panner::set_automation_state),
			(AutoState) Play)));
	pan_astate_menu->items().push_back (MenuElem (_("Write"), sigc::bind (
			sigc::mem_fun (_panner.get(), &Panner::set_automation_state),
			(AutoState) Write)));
	pan_astate_menu->items().push_back (MenuElem (_("Touch"), sigc::bind (
			sigc::mem_fun (_panner.get(), &Panner::set_automation_state),
			(AutoState) Touch)));

}

void
PannerUI::build_astyle_menu ()
{
	using namespace Menu_Helpers;

	if (pan_astyle_menu == 0) {
		pan_astyle_menu = new Menu;
		pan_astyle_menu->set_name ("ArdourContextMenu");
	} else {
		pan_astyle_menu->items().clear();
	}

	pan_astyle_menu->items().push_back (MenuElem (_("Trim")));
	pan_astyle_menu->items().push_back (MenuElem (_("Abs")));
}

boost::shared_ptr<PBD::Controllable>
PannerUI::get_controllable()
{
	return pan_bars[0]->get_controllable();
}

bool
PannerUI::panning_link_button_press (GdkEventButton*)
{
	return true;
}

bool
PannerUI::panning_link_button_release (GdkEventButton*)
{
	if (!ignore_toggle) {
		_panner->set_linked (!_panner->linked());
	}
	return true;
}

void
PannerUI::panning_link_direction_clicked()
{
	switch (_panner->link_direction()) {
	case Panner::SameDirection:
		_panner->set_link_direction (Panner::OppositeDirection);
		break;
	default:
		_panner->set_link_direction (Panner::SameDirection);
		break;
	}
}

void
PannerUI::update_pan_linkage ()
{
	ENSURE_GUI_THREAD (*this, &PannerUI::update_pan_linkage)

	bool const x = _panner->linked();
	bool const bx = panning_link_button.get_active();

	if (x != bx) {

		ignore_toggle = true;
		panning_link_button.set_active (x);
		ignore_toggle = false;
	}

	panning_link_direction_button.set_sensitive (x);

	switch (_panner->link_direction()) {
	case Panner::SameDirection:
	        panning_link_direction_button.set_image (*(manage (new Image (get_xpm ("forwardblarrow.xpm")))));
		break;
	default:
	        panning_link_direction_button.set_image (*(manage (new Image (get_xpm("revdblarrow.xpm")))));
		break;
	}
}

void
PannerUI::on_size_allocate (Allocation& a)
{
        HBox::on_size_allocate (a);
}

void
PannerUI::set_width (Width w)
{
	switch (w) {
	case Wide:
		panning_link_button.set_label (_("link"));
		break;
	case Narrow:
		panning_link_button.set_label (_("L"));
		break;
	}

	_width = w;
}


PannerUI::~PannerUI ()
{
	for (vector<Adjustment*>::iterator i = pan_adjustments.begin(); i != pan_adjustments.end(); ++i) {
		delete (*i);
	}

	for (vector<PannerBar*>::iterator i = pan_bars.begin(); i != pan_bars.end(); ++i) {
		delete (*i);
	}

	delete twod_panner;
	delete big_window;
	delete pan_menu;
	delete pan_astyle_menu;
	delete pan_astate_menu;
        delete _position_fader;
        delete _width_fader;
}


void
PannerUI::panner_changed (void* src)
{
	ENSURE_GUI_THREAD (*this, &PannerUI::panner_changed)

	setup_pan ();

	if (src == this) {
		return;
	}

	switch (_panner->npanners()) {
	case 0:
		panning_link_direction_button.set_sensitive (false);
		panning_link_button.set_sensitive (false);
		return;
	case 1:
		panning_link_direction_button.set_sensitive (false);
		panning_link_button.set_sensitive (false);
		break;
	default:
		panning_link_direction_button.set_sensitive (_panner->linked ());
		panning_link_button.set_sensitive (true);
	}

	uint32_t const nouts = _panner->nouts();

	switch (nouts) {
	case 0:
	case 1:
		/* relax */
		break;

	case 2:
		/* bring pan bar state up to date */
		update_pan_bars (false);
		break;

	default:
		// panner->move_puck (pan_value (pans[0], pans[1]), 0.5);
		break;
	}
}

void
PannerUI::update_pan_state ()
{
	/* currently nothing to do */
	// ENSURE_GUI_THREAD (*this, &PannerUI::update_panner_state)
}

void
PannerUI::setup_pan ()
{
	if (!_panner) {
		return;
	}

	uint32_t const nouts = _panner->nouts();
	uint32_t const npans = _panner->npanners();

	if (int32_t (nouts) == _current_nouts && int32_t (npans) == _current_npans) {
		return;
	}

	_pan_control_connections.drop_connections ();
	for (uint32_t i = 0; i < _panner->npanners(); ++i) {
		connect_to_pan_control (i);
	}

	_current_nouts = nouts;
	_current_npans = npans;

	if (nouts == 0 || nouts == 1) {

		while (!pan_adjustments.empty()) {
			delete pan_bars.back();
			pan_bars.pop_back ();
			delete pan_adjustments.back();
			pan_adjustments.pop_back ();
		}

		delete twod_panner;
		twod_panner = 0;

		/* stick something into the panning viewport so that it redraws */

		EventBox* eb = manage (new EventBox());
		panning_viewport.remove ();
		panning_viewport.add (*eb);
		panning_viewport.show_all ();

	} else if (nouts == 2) {

		vector<Adjustment*>::size_type asz;

		while (!pan_adjustments.empty()) {
			delete pan_bars.back();
			pan_bars.pop_back ();
			delete pan_adjustments.back();
			pan_adjustments.pop_back ();
		}

		delete twod_panner;
		twod_panner = 0;

		while ((asz = pan_adjustments.size()) < npans) {

			float x, rx;
			PannerBar* bc;

			/* initialize adjustment with 0.0 (L) or 1.0 (R) for the first and second panners,
			   which serves as a default, otherwise use current value */

			rx = _panner->pan_control( asz)->get_value();

			if (npans == 1) {
				x = 0.5;
			} else if (asz == 0) {
				x = 0.0;
			} else if (asz == 1) {
				x = 1.0;
			} else {
				x = rx;
			}

			pan_adjustments.push_back (new Adjustment (x, 0, 1.0, 0.005, 0.05));
			bc = new PannerBar (*pan_adjustments[asz],
				boost::static_pointer_cast<PBD::Controllable>( _panner->pan_control( asz )) );

			/* now set adjustment with current value of panner, then connect the signals */
			pan_adjustments.back()->set_value(rx);
			pan_adjustments.back()->signal_value_changed().connect (sigc::bind (sigc::mem_fun(*this, &PannerUI::pan_adjustment_changed), (uint32_t) asz));
			connect_to_pan_control (asz);

			bc->set_name ("PanSlider");
			bc->set_shadow_type (Gtk::SHADOW_NONE);

			boost::shared_ptr<AutomationControl> ac = _panner->pan_control (asz);

			if (asz) {
				bc->StartGesture.connect (sigc::bind (sigc::mem_fun (*this, &PannerUI::start_touch), 
                                                                      boost::weak_ptr<AutomationControl> (ac)));
				bc->StopGesture.connect (sigc::bind (sigc::mem_fun (*this, &PannerUI::stop_touch), 
                                                                     boost::weak_ptr<AutomationControl>(ac)));
			}

			char buf[64];
			snprintf (buf, sizeof (buf), _("panner for channel %zu"), asz + 1);
			ARDOUR_UI::instance()->set_tip (bc->event_widget(), buf);

			bc->event_widget().signal_button_release_event().connect
				(sigc::bind (sigc::mem_fun(*this, &PannerUI::pan_button_event), (uint32_t) asz));

			bc->set_size_request (-1, pan_bar_height);
			bc->SpinnerActive.connect (sigc::mem_fun (*this, &PannerUI::bar_spinner_activate));

			pan_bars.push_back (bc);
			pan_bar_packer.pack_start (*bc, false, false);
		}

		/* now that we actually have the pan bars,
		   set their sensitivity based on current
		   automation state.
		*/

		update_pan_sensitive ();

		panning_viewport.remove ();
		panning_viewport.add (pan_bar_packer);
		panning_viewport.show_all ();

                if (npans == 2) {
                        /* add position and width controls */
                        if (_position_fader == 0) {
                                _position_fader = new BarController (_position_adjustment, _panner->direction_control());
                                _position_fader->set_size_request (-1, pan_bar_height/2);
                                _position_fader->set_name ("PanSlider");
                                _position_fader->set_style (BarController::Line);
                                ARDOUR_UI::instance()->set_tip (_position_fader, _("Pan Position"));                                
                                _position_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &PannerUI::position_adjusted));
                                _panner->direction_control()->Changed.connect (connections, invalidator (*this), boost::bind (&PannerUI::show_position, this), gui_context());
                                show_position();

                                _width_fader = new BarController (_width_adjustment, _panner->width_control());
                                _width_fader->set_size_request (-1, pan_bar_height/2);
                                _width_fader->set_name ("PanSlider");
                                _width_fader->set_style (BarController::CenterOut);
                                ARDOUR_UI::instance()->set_tip (_width_fader, _("Stereo Image Width"));                                
                                _width_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &PannerUI::width_adjusted));
                                 _panner->width_control()->Changed.connect (connections, invalidator (*this), boost::bind (&PannerUI::show_width, this), gui_context());
                                show_width();
                                
                                poswidth_box.pack_start (*_position_fader, true, true);
                                poswidth_box.pack_start (*_width_fader, true, true);
                        }
                        pan_vbox.pack_start (poswidth_box, false, false);
                        poswidth_box.show_all ();
                        cerr << "Packed poswidth and mde it visible\n";
                } else {
                        if (_position_fader) {
                                pan_vbox.remove (poswidth_box);
                                cerr << "Hid poswidth\n";
                        }
                }

	} else {

		if (!twod_panner) {
			twod_panner = new Panner2d (_panner, 61);
			twod_panner->set_name ("MixerPanZone");
			twod_panner->show ();

			twod_panner->signal_button_press_event().connect
				(sigc::bind (sigc::mem_fun(*this, &PannerUI::pan_button_event), (uint32_t) 0), false);
		}

		update_pan_sensitive ();
		twod_panner->reset (npans);
 		if (big_window) {
 			big_window->reset (npans);
 		}
		twod_panner->set_size_request (-1, 61);

		/* and finally, add it to the panner frame */

		panning_viewport.remove ();
		panning_viewport.add (*twod_panner);
		panning_viewport.show_all ();
	}
}

void
PannerUI::start_touch (boost::weak_ptr<AutomationControl> wac)
{
        boost::shared_ptr<AutomationControl> ac = wac.lock();
        if (!ac) {
                return;
        }
        ac->start_touch (ac->session().transport_frame());
}

void
PannerUI::stop_touch (boost::weak_ptr<AutomationControl> wac)
{
        boost::shared_ptr<AutomationControl> ac = wac.lock();
        if (!ac) {
                return;
        }
        ac->stop_touch (false, ac->session().transport_frame());
}

bool
PannerUI::pan_button_event (GdkEventButton* ev, uint32_t which)
{
	switch (ev->button) {
	case 1:
		if (twod_panner && ev->type == GDK_2BUTTON_PRESS) {
			if (!big_window) {
				big_window = new Panner2dWindow (_panner, 400, _panner->npanners());
			}
			big_window->show ();
			return true;
		}
		break;

	case 3:
		if (pan_menu == 0) {
			pan_menu = manage (new Menu);
			pan_menu->set_name ("ArdourContextMenu");
		}
		build_pan_menu (which);
		pan_menu->popup (1, ev->time);
		return true;
		break;
	default:
		return false;
	}

	return false; // what's wrong with gcc?
}

void
PannerUI::build_pan_menu (uint32_t which)
{
	using namespace Menu_Helpers;
	MenuList& items (pan_menu->items());

	items.clear ();

	items.push_back (CheckMenuElem (_("Mute")));

	/* set state first, connect second */

	(dynamic_cast<CheckMenuItem*> (&items.back()))->set_active (_panner->streampanner(which).muted());
	(dynamic_cast<CheckMenuItem*> (&items.back()))->signal_toggled().connect
		(sigc::bind (sigc::mem_fun(*this, &PannerUI::pan_mute), which));

	items.push_back (CheckMenuElem (_("Bypass"), sigc::mem_fun(*this, &PannerUI::pan_bypass_toggle)));
	bypass_menu_item = static_cast<CheckMenuItem*> (&items.back());

	/* set state first, connect second */

	bypass_menu_item->set_active (_panner->bypassed());
	bypass_menu_item->signal_toggled().connect (sigc::mem_fun(*this, &PannerUI::pan_bypass_toggle));

	items.push_back (MenuElem (_("Reset"), sigc::bind (sigc::mem_fun (*this, &PannerUI::pan_reset), which)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Reset all"), sigc::mem_fun (*this, &PannerUI::pan_reset_all)));
}

void
PannerUI::pan_mute (uint32_t which)
{
	StreamPanner& sp = _panner->streampanner(which);
	sp.set_muted (!sp.muted());
}

void
PannerUI::pan_bypass_toggle ()
{
	if (bypass_menu_item && (_panner->bypassed() != bypass_menu_item->get_active())) {
		_panner->set_bypassed (!_panner->bypassed());
	}
}

void
PannerUI::pan_reset (uint32_t which)
{
	_panner->reset_streampanner (which);
}

void
PannerUI::pan_reset_all ()
{
	_panner->reset_to_default ();
}

void
PannerUI::effective_pan_display ()
{
	if (_panner->empty()) {
		return;
	}

	switch (_panner->nouts()) {
	case 0:
	case 1:
		/* relax */
		break;

	case 2:
		update_pan_bars (true);
		break;

	default:
		//panner->move_puck (pan_value (v, right), 0.5);
		break;
	}
}

void
PannerUI::pan_adjustment_changed (uint32_t which)
{
	if (!in_pan_update && which < _panner->npanners()) {

		float val = pan_adjustments[which]->get_value ();
		float const xpos = _panner->pan_control(which)->get_value();

		/* add a kinda-sorta detent for the middle */

		if (val != 0.5 && Panner::equivalent (val, 0.5)) {
			/* this is going to be reentrant, so just
			   return after it.
			*/

			in_pan_update = true;
			pan_adjustments[which]->set_value (0.5);
			in_pan_update = false;
			return;
		}

		if (!Panner::equivalent (val, xpos)) {

			_panner->pan_control(which)->set_value (val);
			/* XXX
			   the panner objects have no access to the session,
			   so do this here. ick.
			*/
			_session->set_dirty();
		}
	}
}

void
PannerUI::pan_value_changed (uint32_t which)
{
	ENSURE_GUI_THREAD (*this, &PannerUI::pan_value_changed, which)

	if (twod_panner) {

		in_pan_update = true;
		twod_panner->move_puck (which, _panner->streampanner(which).get_position());
		in_pan_update = false;

	} else if (_panner->npanners() > 0 && which < _panner->npanners()) {
                AngularVector model = _panner->streampanner(which).get_position();
                double fract = pan_adjustments[which]->get_value();
                AngularVector view (BaseStereoPanner::lr_fract_to_azimuth (fract), 0.0);

		if (!Panner::equivalent (model, view)) {
			in_pan_update = true;
			pan_adjustments[which]->set_value (BaseStereoPanner::azimuth_to_lr_fract (model.azi));
			in_pan_update = false;
		}
	}
}

void
PannerUI::update_pan_bars (bool only_if_aplay)
{
	uint32_t n;
	vector<Adjustment*>::iterator i;

	in_pan_update = true;

	/* this runs during automation playback, and moves the bar controllers
	   and/or pucks around.
	*/

	for (i = pan_adjustments.begin(), n = 0; i != pan_adjustments.end(); ++i, ++n) {

		if (only_if_aplay) {
			boost::shared_ptr<AutomationList> alist (_panner->streampanner(n).pan_control()->alist());

			if (!alist->automation_playback()) {
				continue;
			}
		}

                AngularVector model = _panner->streampanner(n).get_effective_position();
                double fract = (*i)->get_value();
                AngularVector view (BaseStereoPanner::lr_fract_to_azimuth (fract), 0.0);

		if (!Panner::equivalent (model, view)) {
			(*i)->set_value (BaseStereoPanner::azimuth_to_lr_fract (model.azi));
		}
	}

	in_pan_update = false;
}

void
PannerUI::update_pan_sensitive ()
{
	bool const sensitive = !(_panner->mono()) && !(_panner->automation_state() & Play);

	switch (_panner->nouts()) {
	case 0:
	case 1:
		break;
	case 2:
		for (vector<PannerBar*>::iterator i = pan_bars.begin(); i != pan_bars.end(); ++i) {
			(*i)->set_sensitive (sensitive);
		}
		break;
	default:
		if (twod_panner) {
			twod_panner->set_sensitive (sensitive);
		}
		if (big_window) {
			big_window->set_sensitive (sensitive);
		}
		break;
	}
}

gint
PannerUI::pan_automation_state_button_event (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (ev->type == GDK_BUTTON_RELEASE) {
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		if (pan_astate_menu == 0) {
			build_astate_menu ();
		}
		pan_astate_menu->popup (1, ev->time);
		break;
	default:
		break;
	}

	return TRUE;
}

gint
PannerUI::pan_automation_style_button_event (GdkEventButton *ev)
{
	if (ev->type == GDK_BUTTON_RELEASE) {
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		if (pan_astyle_menu == 0) {
			build_astyle_menu ();
		}
		pan_astyle_menu->popup (1, ev->time);
		break;
	default:
		break;
	}
	return TRUE;
}

void
PannerUI::pan_automation_style_changed ()
{
	ENSURE_GUI_THREAD (*this, &PannerUI::pan_automation_style_changed)

	switch (_width) {
	case Wide:
	        pan_automation_style_button.set_label (astyle_string(_panner->automation_style()));
		break;
	case Narrow:
	  	pan_automation_style_button.set_label (short_astyle_string(_panner->automation_style()));
		break;
	}
}

void
PannerUI::pan_automation_state_changed ()
{
	ENSURE_GUI_THREAD (*this, &PannerUI::pan_automation_state_changed)

	bool x;

	switch (_width) {
	case Wide:
	  pan_automation_state_button.set_label (astate_string(_panner->automation_state()));
		break;
	case Narrow:
	  pan_automation_state_button.set_label (short_astate_string(_panner->automation_state()));
		break;
	}

	/* when creating a new session, we get to create busses (and
	   sometimes tracks) with no outputs by the time they get
	   here.
	*/

	if (_panner->empty()) {
		return;
	}

	x = (_panner->streampanner(0).pan_control()->alist()->automation_state() != Off);

	if (pan_automation_state_button.get_active() != x) {
	ignore_toggle = true;
		pan_automation_state_button.set_active (x);
		ignore_toggle = false;
	}

	update_pan_sensitive ();

	/* start watching automation so that things move */

	pan_watching.disconnect();

	if (x) {
		pan_watching = ARDOUR_UI::RapidScreenUpdate.connect (sigc::mem_fun (*this, &PannerUI::effective_pan_display));
	}
}

string
PannerUI::astate_string (AutoState state)
{
	return _astate_string (state, false);
}

string
PannerUI::short_astate_string (AutoState state)
{
	return _astate_string (state, true);
}

string
PannerUI::_astate_string (AutoState state, bool shrt)
{
	string sstr;

	switch (state) {
	case Off:
		sstr = (shrt ? "M" : _("M"));
		break;
	case Play:
		sstr = (shrt ? "P" : _("P"));
		break;
	case Touch:
		sstr = (shrt ? "T" : _("T"));
		break;
	case Write:
		sstr = (shrt ? "W" : _("W"));
		break;
	}

	return sstr;
}

string
PannerUI::astyle_string (AutoStyle style)
{
	return _astyle_string (style, false);
}

string
PannerUI::short_astyle_string (AutoStyle style)
{
	return _astyle_string (style, true);
}

string
PannerUI::_astyle_string (AutoStyle style, bool shrt)
{
	if (style & Trim) {
		return _("Trim");
	} else {
	        /* XXX it might different in different languages */

		return (shrt ? _("Abs") : _("Abs"));
	}
}

void
PannerUI::set_mono (bool yn)
{
	_panner->set_mono (yn);
	update_pan_sensitive ();
}


void
PannerUI::connect_to_pan_control (uint32_t i)
{
	_panner->pan_control(i)->Changed.connect (
		_pan_control_connections, invalidator (*this), boost::bind (&PannerUI::pan_value_changed, this, i), gui_context ()
		);
}

void
PannerUI::bar_spinner_activate (bool a)
{
	_bar_spinner_active = a;
}

void
PannerUI::setup_slider_pix ()
{
	_poswidth_slider = ::get_icon ("fader_belt_h_thin");
	assert (_poswidth_slider);
}

void
PannerUI::show_width ()
{
        float const value = _panner->width_control()->get_value ();
        
	if (_width_adjustment.get_value() != value) {
		_ignore_width_change = true;
		_width_adjustment.set_value (value);
		_ignore_width_change = false;
	}
}

void
PannerUI::width_adjusted ()
{
	if (_ignore_width_change) {
		return;
	}

	_panner->width_control()->set_value (_width_adjustment.get_value());
}

void
PannerUI::show_position ()
{
        float const value = _panner->direction_control()->get_value ();
        
	if (_position_adjustment.get_value() != value) {
		_ignore_position_change = true;
		_position_adjustment.set_value (value);
		_ignore_position_change = false;
	}
}

void
PannerUI::position_adjusted ()
{
	if (_ignore_position_change) {
		return;
	}

	_panner->direction_control()->set_value (_position_adjustment.get_value());
}
