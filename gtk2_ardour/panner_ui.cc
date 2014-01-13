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

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/barcontroller.h>

#include "pbd/fastlog.h"

#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/session.h"

#include "ardour_ui.h"
#include "panner_ui.h"
#include "panner2d.h"
#include "utils.h"
#include "gui_thread.h"
#include "stereo_panner.h"
#include "mono_panner.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;

const int PannerUI::pan_bar_height = 35;

PannerUI::PannerUI (Session* s)
	: _current_nouts (-1)
	, _current_nins (-1)
	, pan_automation_style_button ("")
	, pan_automation_state_button ("")
	, _panner_list()
{
	set_session (s);

	ignore_toggle = false;
	pan_menu = 0;
	pan_astate_menu = 0;
	pan_astyle_menu = 0;
	in_pan_update = false;
        _stereo_panner = 0;
	_mono_panner = 0;
        _ignore_width_change = false;
        _ignore_position_change = false;

	pan_automation_style_button.set_name ("MixerAutomationModeButton");
	pan_automation_state_button.set_name ("MixerAutomationPlaybackButton");

	ARDOUR_UI::instance()->set_tip (pan_automation_state_button, _("Pan automation mode"));
	ARDOUR_UI::instance()->set_tip (pan_automation_style_button, _("Pan automation type"));

	//set_size_request_to_display_given_text (pan_automation_state_button, X_("O"), 2, 2);
	//set_size_request_to_display_given_text (pan_automation_style_button, X_("0"), 2, 2);

	pan_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	pan_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	pan_automation_style_button.signal_button_press_event().connect (sigc::mem_fun(*this, &PannerUI::pan_automation_style_button_event), false);
	pan_automation_state_button.signal_button_press_event().connect (sigc::mem_fun(*this, &PannerUI::pan_automation_state_button_event), false);

	pan_vbox.set_spacing (2);
	pack_start (pan_vbox, true, true);

	twod_panner = 0;
	big_window = 0;

	set_width(Narrow);
}

void
PannerUI::set_panner (boost::shared_ptr<PannerShell> ps, boost::shared_ptr<Panner> p)
{
        /* note that the panshell might not change here (i.e. ps == _panshell)
         */

 	connections.drop_connections ();

	delete pan_astyle_menu;
	pan_astyle_menu = 0;

	delete pan_astate_menu;
	pan_astate_menu = 0;

        _panshell = ps;
	_panner = p;

	delete twod_panner;
	twod_panner = 0;

        delete _stereo_panner;
        _stereo_panner = 0;

        delete _mono_panner;
        _mono_panner = 0;

	if (!_panner) {
		return;
	}

	_panshell->Changed.connect (connections, invalidator (*this), boost::bind (&PannerUI::panshell_changed, this), gui_context());

        /* new panner object, force complete reset of panner GUI
         */

        _current_nouts = 0;
        _current_nins = 0;

        setup_pan ();
	update_pan_sensitive ();
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

	/** TRANSLATORS: this is `Manual' in the sense of automation not being played,
	    so that changes to pan must be done by hand.
	*/
	pan_astate_menu->items().push_back (MenuElem (S_("Automation|Manual"), sigc::bind (
			sigc::mem_fun (_panner.get(), &Panner::set_automation_state),
			(AutoState) ARDOUR::Off)));
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

void
PannerUI::on_size_allocate (Allocation& a)
{
        HBox::on_size_allocate (a);
}

void
PannerUI::set_width (Width w)
{
	_width = w;
}

PannerUI::~PannerUI ()
{
	delete twod_panner;
	delete big_window;
	delete pan_menu;
	delete pan_astyle_menu;
	delete pan_astate_menu;
        delete _stereo_panner;
        delete _mono_panner;
}

void
PannerUI::panshell_changed ()
{
	set_panner (_panshell, _panshell->panner());
	setup_pan ();
}

void
PannerUI::setup_pan ()
{
	int const nouts = _panner ? _panner->out().n_audio() : -1;
	int const nins = _panner ? _panner->in().n_audio() : -1;

	if (nouts == _current_nouts && nins == _current_nins) {
		return;
	}

        _current_nins = nins;
        _current_nouts = nouts;

        container_clear (pan_vbox);

        delete twod_panner;
        twod_panner = 0;
        delete _stereo_panner;
        _stereo_panner = 0;
	delete _mono_panner;
	_mono_panner = 0;

	if (!_panner) {
		delete big_window;
		big_window = 0;
		return;
	}

	if (_panshell->panner_gui_uri() == "http://ardour.org/plugin/panner_2in2out#ui")
	{
		delete big_window;
		big_window = 0;

		boost::shared_ptr<Pannable> pannable = _panner->pannable();

		_stereo_panner = new StereoPanner (_panshell);
		_stereo_panner->set_size_request (-1, pan_bar_height);
		pan_vbox.pack_start (*_stereo_panner, false, false);

		boost::shared_ptr<AutomationControl> ac;

		ac = pannable->pan_azimuth_control;
		_stereo_panner->StartPositionGesture.connect (sigc::bind (sigc::mem_fun (*this, &PannerUI::start_touch),
					boost::weak_ptr<AutomationControl> (ac)));
		_stereo_panner->StopPositionGesture.connect (sigc::bind (sigc::mem_fun (*this, &PannerUI::stop_touch),
					boost::weak_ptr<AutomationControl>(ac)));

		ac = pannable->pan_width_control;
		_stereo_panner->StartWidthGesture.connect (sigc::bind (sigc::mem_fun (*this, &PannerUI::start_touch),
					boost::weak_ptr<AutomationControl> (ac)));
		_stereo_panner->StopWidthGesture.connect (sigc::bind (sigc::mem_fun (*this, &PannerUI::stop_touch),
					boost::weak_ptr<AutomationControl>(ac)));
		_stereo_panner->signal_button_release_event().connect (sigc::mem_fun(*this, &PannerUI::pan_button_event));
	}
	else if (_panshell->panner_gui_uri() == "http://ardour.org/plugin/panner_1in2out#ui"
			|| _panshell->panner_gui_uri() == "http://ardour.org/plugin/panner_balance#ui")
	{
		delete big_window;
		big_window = 0;
		boost::shared_ptr<Pannable> pannable = _panner->pannable();
		boost::shared_ptr<AutomationControl> ac = pannable->pan_azimuth_control;

		_mono_panner = new MonoPanner (_panshell);

		_mono_panner->StartGesture.connect (sigc::bind (sigc::mem_fun (*this, &PannerUI::start_touch),
					boost::weak_ptr<AutomationControl> (ac)));
		_mono_panner->StopGesture.connect (sigc::bind (sigc::mem_fun (*this, &PannerUI::stop_touch),
					boost::weak_ptr<AutomationControl>(ac)));

		_mono_panner->signal_button_release_event().connect (sigc::mem_fun(*this, &PannerUI::pan_button_event));

		_mono_panner->set_size_request (-1, pan_bar_height);

		update_pan_sensitive ();
		pan_vbox.pack_start (*_mono_panner, false, false);
	}
	else if (_panshell->panner_gui_uri() == "http://ardour.org/plugin/panner_vbap#ui")
	{
		if (!twod_panner) {
			twod_panner = new Panner2d (_panshell, 61);
			twod_panner->set_name ("MixerPanZone");
			twod_panner->show ();
			twod_panner->signal_button_press_event().connect (sigc::mem_fun(*this, &PannerUI::pan_button_event), false);
		}

		update_pan_sensitive ();
		twod_panner->reset (nins);
		if (big_window) {
			big_window->reset (nins);
		}
		twod_panner->set_size_request (-1, 61);

		/* and finally, add it to the panner frame */

		pan_vbox.pack_start (*twod_panner, false, false);
	}
	else
	{
		/* stick something into the panning viewport so that it redraws */
		EventBox* eb = manage (new EventBox());
		pan_vbox.pack_start (*eb, false, false);

		delete big_window;
		big_window = 0;
	}

	pan_vbox.show_all ();
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
PannerUI::pan_button_event (GdkEventButton* ev)
{
	switch (ev->button) {
	case 1:
		if (twod_panner && ev->type == GDK_2BUTTON_PRESS) {
			if (!big_window) {
				big_window = new Panner2dWindow (_panshell, 400, _panner->in().n_audio());
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
		build_pan_menu ();
		pan_menu->popup (1, ev->time);
		return true;
		break;
	default:
		return false;
	}

	return false; // what's wrong with gcc?
}

void
PannerUI::build_pan_menu ()
{
	using namespace Menu_Helpers;
	MenuList& items (pan_menu->items());

	items.clear ();

	items.push_back (CheckMenuElem (_("Bypass"), sigc::mem_fun(*this, &PannerUI::pan_bypass_toggle)));
	bypass_menu_item = static_cast<CheckMenuItem*> (&items.back());

	/* set state first, connect second */

	bypass_menu_item->set_active (_panshell->bypassed());
	bypass_menu_item->signal_toggled().connect (sigc::mem_fun(*this, &PannerUI::pan_bypass_toggle));

	if (!_panshell->bypassed()) {
		items.push_back (MenuElem (_("Reset"), sigc::mem_fun (*this, &PannerUI::pan_reset)));
		items.push_back (MenuElem (_("Edit..."), sigc::mem_fun (*this, &PannerUI::pan_edit)));
	}

	if (_route && _panner_list.size() > 1 && !_panshell->bypassed()) {
		RadioMenuItem::Group group;
		items.push_back (SeparatorElem());

		assert(_panshell->user_selected_panner_uri() == ""
				||  _panshell->user_selected_panner_uri() == _panshell->current_panner_uri());

		_suspend_menu_callbacks = true;
		for (std::map<std::string,std::string>::const_iterator p = _panner_list.begin(); p != _panner_list.end(); ++p) {
			items.push_back (RadioMenuElem (group, p->second,
						sigc::bind(sigc::mem_fun (*this, &PannerUI::pan_set_custom_type), p->first)));
			RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&items.back ());
			i->set_active (_panshell->current_panner_uri() == p->first);
		}
		_suspend_menu_callbacks = false;
	}
}

void
PannerUI::pan_bypass_toggle ()
{
	if (bypass_menu_item && (_panshell->bypassed() != bypass_menu_item->get_active())) {
		_panshell->set_bypassed (!_panshell->bypassed());
	}
}

void
PannerUI::pan_edit ()
{
	if (_panshell->bypassed()) {
		return;
	}
	if (_mono_panner) {
		_mono_panner->edit ();
	} else if (_stereo_panner) {
		_stereo_panner->edit ();
	} else if (twod_panner) {
		if (!big_window) {
			big_window = new Panner2dWindow (_panshell, 400, _panner->in().n_audio());
		}
		big_window->show ();
	}
}

void
PannerUI::pan_reset ()
{
	if (_panshell->bypassed()) {
		return;
	}
	_panner->reset ();
}

void
PannerUI::pan_set_custom_type (std::string uri) {
	if (_suspend_menu_callbacks) return;
	if (_route) {
		_route->set_custom_panner_uri(uri);
	}
}

void
PannerUI::effective_pan_display ()
{
        if (_stereo_panner) {
                _stereo_panner->queue_draw ();
        } else if (_mono_panner) {
                _mono_panner->queue_draw ();
        } else if (twod_panner) {
                twod_panner->queue_draw ();
        }
}

void
PannerUI::update_pan_sensitive ()
{
	bool const sensitive = !(_panner->pannable()->automation_state() & Play);

        pan_vbox.set_sensitive (sensitive);

        if (big_window) {
                big_window->set_sensitive (sensitive);
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
        boost::shared_ptr<Pannable> pannable (_panner->pannable());

	switch (_width) {
	case Wide:
                pan_automation_state_button.set_label (astate_string(pannable->automation_state()));
		break;
	case Narrow:
                pan_automation_state_button.set_label (short_astate_string(pannable->automation_state()));
		break;
	}

	bool x = (pannable->automation_state() != ARDOUR::Off);

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
	case ARDOUR::Off:
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
PannerUI::show_width ()
{
}

void
PannerUI::width_adjusted ()
{
}

void
PannerUI::show_position ()
{
}

void
PannerUI::position_adjusted ()
{
}

void
PannerUI::set_available_panners(boost::shared_ptr<ARDOUR::Route> r, std::map<std::string,std::string> p)
{
	_route = r;
	_panner_list = p;
}
