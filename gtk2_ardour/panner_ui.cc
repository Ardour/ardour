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
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/barcontroller.h>
#include "midi++/manager.h"
#include "pbd/fastlog.h"
#include "pbd/stacktrace.h"

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
using namespace sigc;

const int PannerUI::pan_bar_height = 30;

PannerUI::PannerUI (Session& s)
	: _session (s),
	  hAdjustment(0.0, 0.0, 0.0),
	  vAdjustment(0.0, 0.0, 0.0),
	  panning_viewport(hAdjustment, vAdjustment),
	  panning_up_arrow (Gtk::ARROW_UP, Gtk::SHADOW_OUT),
	  panning_down_arrow (Gtk::ARROW_DOWN, Gtk::SHADOW_OUT),
	  panning_link_button (_("link")),
	  pan_automation_style_button (""),
	  pan_automation_state_button ("")
{
	ignore_toggle = false;
	pan_menu = 0;
	pan_astate_menu = 0;
	pan_astyle_menu = 0;
	in_pan_update = false;

	pan_automation_style_button.set_name ("MixerAutomationModeButton");
	pan_automation_state_button.set_name ("MixerAutomationPlaybackButton");

	ARDOUR_UI::instance()->tooltips().set_tip (pan_automation_state_button, _("Pan automation mode"));
	ARDOUR_UI::instance()->tooltips().set_tip (pan_automation_style_button, _("Pan automation type"));

	//set_size_request_to_display_given_text (pan_automation_state_button, X_("O"), 2, 2);
	//set_size_request_to_display_given_text (pan_automation_style_button, X_("0"), 2, 2);

	pan_bar_packer.set_size_request (-1, 61);
	panning_viewport.set_size_request (-1, 61);
	panning_viewport.set_name (X_("BaseFrame"));

	ARDOUR_UI::instance()->tooltips().set_tip (panning_link_button,
						   _("panning link control"));
	ARDOUR_UI::instance()->tooltips().set_tip (panning_link_direction_button,
						   _("panning link direction"));

	pan_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	pan_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	pan_automation_style_button.signal_button_press_event().connect (mem_fun(*this, &PannerUI::pan_automation_style_button_event), false);
	pan_automation_state_button.signal_button_press_event().connect (mem_fun(*this, &PannerUI::pan_automation_state_button_event), false);

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
		(mem_fun(*this, &PannerUI::panning_link_direction_clicked));

	panning_link_button.signal_button_press_event().connect
		(mem_fun(*this, &PannerUI::panning_link_button_press), false);
	panning_link_button.signal_button_release_event().connect
		(mem_fun(*this, &PannerUI::panning_link_button_release), false);

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

	panner = 0;
	big_window = 0;

	set_width(Narrow);
}
  
void
PannerUI::set_panner (boost::shared_ptr<Panner> p)
{
 	connections.clear ();
	
	delete pan_astyle_menu;
	pan_astyle_menu = 0;

	delete pan_astate_menu;
	pan_astate_menu = 0;
 			
	_panner = p;

	delete panner;
	panner = 0;

	if (!_panner) {
		return;
	}
 
 	connections.push_back (_panner->Changed.connect (mem_fun(*this, &PannerUI::panner_changed)));
 	connections.push_back (_panner->LinkStateChanged.connect (mem_fun(*this, &PannerUI::update_pan_linkage)));
 	connections.push_back (_panner->StateChanged.connect (mem_fun(*this, &PannerUI::update_pan_state)));
 
	setup_pan ();

	pan_changed (0);
	update_pan_sensitive ();
	update_pan_linkage ();
	pan_automation_state_changed ();

#if WHERE_DOES_THIS_LIVE	
	pan_bar_packer.show();
	panning_viewport.show();
	panning_up.show();
	panning_up_arrow.show();
	panning_down.show();
	panning_down_arrow.show();
	pan_vbox.show();
	panning_link_button.show();
	panning_link_direction_button.show();
	panning_link_box.show();
	pan_automation_style_button.show();
	pan_automation_state_button.show();
	show();
#endif
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

	pan_astate_menu->items().push_back (MenuElem (_("Manual"), bind (
			mem_fun (_panner.get(), &Panner::set_automation_state),
			(AutoState) Off)));
	pan_astate_menu->items().push_back (MenuElem (_("Play"), bind (
			mem_fun (_panner.get(), &Panner::set_automation_state),
			(AutoState) Play)));
	pan_astate_menu->items().push_back (MenuElem (_("Write"), bind (
			mem_fun (_panner.get(), &Panner::set_automation_state),
			(AutoState) Write)));
	pan_astate_menu->items().push_back (MenuElem (_("Touch"), bind (
			mem_fun (_panner.get(), &Panner::set_automation_state),
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
PannerUI::panning_link_button_press (GdkEventButton* ev)
{
	return true;
}

bool
PannerUI::panning_link_button_release (GdkEventButton* ev)
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
	ENSURE_GUI_THREAD(mem_fun(*this, &PannerUI::update_pan_linkage));
	
	bool x = _panner->linked();
	bool bx = panning_link_button.get_active();

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

	delete panner;
	delete big_window;
	delete pan_menu;
	delete pan_astyle_menu;
	delete pan_astate_menu;
}


void
PannerUI::panner_changed ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &PannerUI::panner_changed));
	setup_pan ();
	pan_changed (0);
}

void
PannerUI::update_pan_state ()
{
	/* currently nothing to do */
	// ENSURE_GUI_THREAD (mem_fun(*this, &PannerUI::update_panner_state));
}

void
PannerUI::setup_pan ()
{
	cerr << "Setup pan for " << _panner->name() << endl;
	// PBD::stacktrace (cerr, 5);

	if (!_panner) {
		return;
	}

	uint32_t nouts = _panner->nouts();

	cerr << "\tnouts = " << nouts << endl;

	if (nouts == 0 || nouts == 1) {

		while (!pan_adjustments.empty()) {
			delete pan_bars.back();
			pan_bars.pop_back ();
			delete pan_adjustments.back();
			pan_adjustments.pop_back ();
		}

		/* stick something into the panning viewport so that it redraws */

		EventBox* eb = manage (new EventBox());
		panning_viewport.remove ();
		panning_viewport.add (*eb);
		panning_viewport.show_all ();

	} else if (nouts == 2) {

		vector<Adjustment*>::size_type asz;
		uint32_t npans = _panner->npanners();

		while (!pan_adjustments.empty()) {
			delete pan_bars.back();
			pan_bars.pop_back ();
			delete pan_adjustments.back();
			pan_adjustments.pop_back ();
		}

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
			pan_adjustments.back()->signal_value_changed().connect (bind (mem_fun(*this, &PannerUI::pan_adjustment_changed), (uint32_t) asz));

			_panner->pan_control( asz )->Changed.connect (bind (mem_fun(*this, &PannerUI::pan_value_changed), (uint32_t) asz));

			
			bc->set_name ("PanSlider");
			bc->set_shadow_type (Gtk::SHADOW_NONE);
			
			boost::shared_ptr<AutomationControl> ac = _panner->pan_control (asz);
			
			if (asz) {
				bc->StartGesture.connect (mem_fun (*ac, &AutomationControl::start_touch));
				bc->StopGesture.connect (mem_fun (*ac, &AutomationControl::stop_touch));
			}

			char buf[64];
			snprintf (buf, sizeof (buf), _("panner for channel %zu"), asz + 1);
			ARDOUR_UI::instance()->tooltips().set_tip (bc->event_widget(), buf);

			bc->event_widget().signal_button_release_event().connect
				(bind (mem_fun(*this, &PannerUI::pan_button_event), (uint32_t) asz));

			bc->set_size_request (-1, pan_bar_height);

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

	} else {

		if (!panner) {
			panner = new Panner2d (_panner, 61);
			panner->set_name ("MixerPanZone");
			panner->show ();
 
 			panner->signal_button_press_event().connect
 				(bind (mem_fun(*this, &PannerUI::pan_button_event), (uint32_t) 0), false);
		}
		
		update_pan_sensitive ();
		panner->reset (nouts);
 		if (big_window) {
 			big_window->reset (_panner->npanners());
 		}
		panner->set_size_request (-1, 61);

		/* and finally, add it to the panner frame */

		panning_viewport.remove ();
		panning_viewport.add (*panner);
		panning_viewport.show_all ();
	}
}

bool
PannerUI::pan_button_event (GdkEventButton* ev, uint32_t which)
{
	switch (ev->button) {
	case 1:
		if (panner && ev->type == GDK_2BUTTON_PRESS) {
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
		(bind (mem_fun(*this, &PannerUI::pan_mute), which));

	items.push_back (CheckMenuElem (_("Bypass"), mem_fun(*this, &PannerUI::pan_bypass_toggle)));
	bypass_menu_item = static_cast<CheckMenuItem*> (&items.back());

	/* set state first, connect second */

	bypass_menu_item->set_active (_panner->bypassed());
	bypass_menu_item->signal_toggled().connect (mem_fun(*this, &PannerUI::pan_bypass_toggle));

	items.push_back (MenuElem (_("Reset"), bind (mem_fun (*this, &PannerUI::pan_reset), which)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Reset all"), mem_fun (*this, &PannerUI::pan_reset_all)));
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
PannerUI::pan_changed (void *src)
{
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
		panning_link_direction_button.set_sensitive (true);
		panning_link_button.set_sensitive (true);
	}

	uint32_t nouts = _panner->nouts();

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
PannerUI::pan_adjustment_changed (uint32_t which)
{
	if (!in_pan_update && which < _panner->npanners()) {

		float xpos;
		float val = pan_adjustments[which]->get_value ();
		xpos = _panner->pan_control( which )->get_value();

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

			_panner->streampanner(which).set_position (val);
			/* XXX 
			   the panner objects have no access to the session,
			   so do this here. ick.
			*/
			_session.set_dirty();
		}
	}
}

void
PannerUI::pan_value_changed (uint32_t which)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &PannerUI::pan_value_changed), which));
							   
	if (_panner->npanners() > 1 && which < _panner->npanners()) {
		float xpos;
		float val = pan_adjustments[which]->get_value ();

		_panner->streampanner(which).get_position (xpos);

		if (!Panner::equivalent (val, xpos)) {
			in_pan_update = true;
			pan_adjustments[which]->set_value (xpos);
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
		float xpos, val;

		if (only_if_aplay) {
			boost::shared_ptr<AutomationList> alist (_panner->streampanner(n).pan_control()->alist());
			
			if (!alist->automation_playback()) {
				continue;
			}
		}

		_panner->streampanner(n).get_effective_position (xpos);
		val = (*i)->get_value ();
		
		if (!Panner::equivalent (val, xpos)) {
			(*i)->set_value (xpos);
		}
	}

	in_pan_update = false;
}

void
PannerUI::update_pan_sensitive () 
{
	bool sensitive = !(_panner->automation_state() & Play);

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
		if (panner) {
			panner->set_sensitive (sensitive);
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
	ENSURE_GUI_THREAD(mem_fun(*this, &PannerUI::pan_automation_style_changed));
	
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
	ENSURE_GUI_THREAD(mem_fun(*this, &PannerUI::pan_automation_state_changed));
	
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
		pan_watching = ARDOUR_UI::RapidScreenUpdate.connect (mem_fun (*this, &PannerUI::effective_pan_display));
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
