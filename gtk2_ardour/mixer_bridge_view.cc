/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <map>
#include <sigc++/bind.h>

#include <gtkmm/accelmap.h>

#include <glibmm/threads.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "ardour/debug.h"
#include "ardour/midi_track.h"
#include "ardour/route_group.h"
#include "ardour/session.h"

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

#include "mixer_bridge_view.h"

#include "keyboard.h"
#include "monitor_section.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "utils.h"
#include "route_sorter.h"
#include "actions.h"
#include "gui_thread.h"
#include "global_signals.h"
#include "meter_patterns.h"
#include "waves_grid.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

using PBD::atoi;

struct SignalOrderRouteSorter {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		if (a->is_master() || a->is_monitor() || !boost::dynamic_pointer_cast<Track>(a)) {
			/* "a" is a special route (master, monitor, etc), and comes
			 * last in the mixer ordering
			 */
			return false;
		} else if (b->is_master() || b->is_monitor() || !boost::dynamic_pointer_cast<Track>(b)) {
			/* everything comes before b */
			return true;
		}
		return a->order_key () < b->order_key ();
	}
};

MixerBridgeView::MixerBridgeView (const std::string& mixer_bridge_script_name, const std::string& mixer_strip_script_name)
	: Gtk::EventBox()
	, WavesUI (mixer_bridge_script_name, *this)
	, _mixer_strips_home (get_container ("mixer_strips_home"))
    , _scroll (get_scrolled_window ("scroller"))
   	, _following_editor_selection (false)
	, _mixer_strip_script_name (mixer_strip_script_name)
{
	set_attributes (*this, *xml_tree ()->root (), XMLNodeMap ());
	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));
	Route::SyncOrderKeys.connect (*this, invalidator (*this), boost::bind (&MixerBridgeView::sync_order_keys, this), gui_context());
	MixerStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&MixerBridgeView::remove_strip, this, _1), gui_context());
    MixerStrip::EndStripNameEdit.connect (*this, invalidator (*this), boost::bind (&MixerBridgeView::begin_strip_name_edit, this, _1, _2), gui_context());


	if (dynamic_cast <WavesGrid*> (&_mixer_strips_home)) {
		_mixer_strips_home.get_parent()->signal_size_allocate().connect (sigc::mem_fun(*this, &MixerBridgeView::parent_on_size_allocate));
	}
}

MixerBridgeView::~MixerBridgeView ()
{
}

void
MixerBridgeView::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	boost::shared_ptr<RouteList> routes = _session->get_routes();

	add_strips(*routes);

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&MixerBridgeView::add_strips, this, _1), gui_context());

	start_updating ();
}

void
MixerBridgeView::track_editor_selection ()
{
	PublicEditor::instance().get_selection().TracksChanged.connect (sigc::mem_fun (*this, &MixerBridgeView::follow_editor_selection));
}

void
MixerBridgeView::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &MixerBridgeView::session_going_away);

	for (std::map <boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		delete (*i).second;
	}

	_strips.clear ();
	stop_updating ();

	SessionHandlePtr::session_going_away ();
	_session = 0;
}


void
MixerBridgeView::all_gain_sliders_set_visible (bool visibility)
{
    for (std::map <boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
        (*i).second->gain_slider_set_visible (visibility);
    }
}

gint
MixerBridgeView::start_updating ()
{
	fast_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (sigc::mem_fun(*this, &MixerBridgeView::fast_update_strips));
	return 0;
}

gint
MixerBridgeView::stop_updating ()
{
	fast_screen_update_connection.disconnect();
	return 0;
}

void
MixerBridgeView::fast_update_strips ()
{
	if (!is_mapped () || !_session) {
		return;
	}
	for (std::map <boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		(*i).second->fast_update ();
	}
}

void
MixerBridgeView::add_strips (RouteList& routes)
{
	// First detach all the prviously added strips from the ui tree.
	for (std::map<boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		_mixer_strips_home.remove (*(*i).second); // we suppose _mixer_strips_home is
		                                          // the parnet. 
	}

	// Now create the strips for newly added routes
	for (RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master() ||
			!boost::dynamic_pointer_cast<Track> (route)) {
			continue;
		}

		MixerStrip* strip = new MixerStrip (_session, route, _mixer_strip_script_name, _max_name_size);
		strip->signal_button_press_event().connect (sigc::bind (sigc::mem_fun(*this, &MixerBridgeView::strip_button_release_event), strip));
        
        // in Multi-Out mode, new created strip mustn't show gain slider
        bool set_gain_slider_visible = Config->get_output_auto_connect() & AutoConnectMaster; 
        strip->gain_slider_set_visible (set_gain_slider_visible);
        
        //strip->EndStripNameEdit.connect (*this, invalidator (*this), (sigc::bind (sigc::mem_fun(*this, &MixerBridgeView::begin_strip_name_edit), strip)), gui_context());
        
		_strips [route] = strip;
		strip->show();
	}

	// Now sort the session's routes and pack the strips accordingly
	SignalOrderRouteSorter sorter;
	RouteList copy(*_session->get_routes());
	copy.sort(sorter);

	for (RouteList::iterator x = copy.begin(); x != copy.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master() ||
			!boost::dynamic_pointer_cast<Track> (route)) {
			continue;
		}
		Gtk::Box* the_box = dynamic_cast <Gtk::Box*> (&_mixer_strips_home);
		WavesGrid* the_grid = dynamic_cast <WavesGrid*> (&_mixer_strips_home);

		std::map <boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.find (route);
		if (i != _strips.end ()) {
			if (the_box) {
				the_box->pack_start (*(*i).second, false, false);
			} else {
				if (the_grid) {
					the_grid->pack (*(*i).second);
				}
			}
		}
	}
}

void
MixerBridgeView::remove_strip (MixerStrip* strip)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	boost::shared_ptr<ARDOUR::Route> route = strip->route ();
	std::map <boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.find (route);
	if (i != _strips.end ()) {
		_strips.erase (i);
	}
}

void
MixerBridgeView::sync_order_keys ()
{
	Glib::Threads::Mutex::Lock lm (_resync_mutex);

	if (!_session) {
		return;
	}

	SignalOrderRouteSorter sorter;
	boost::shared_ptr<RouteList> routes = _session->get_routes();

	for (std::map<boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		_mixer_strips_home.remove (*(*i).second); // we suppose _mixer_strips_home is
		                                          // the parnet. 
	}

	RouteList copy(*routes);
	copy.sort(sorter);

		Gtk::Box* the_box = dynamic_cast <Gtk::Box*> (&_mixer_strips_home);
		WavesGrid* the_grid = dynamic_cast <WavesGrid*> (&_mixer_strips_home);
	for (RouteList::iterator x = copy.begin(); x != copy.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner() || route->is_monitor() || route->is_master() ||
			!boost::dynamic_pointer_cast<Track> (route)) {
			continue;
		}
		std::map <boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.find (route);
		if (i != _strips.end ()) {
			if (the_box) {
				the_box->pack_start (*(*i).second, false, false);
			} else {
				if (the_grid) {
					the_grid->pack (*(*i).second);
				}
			}
		}
	}
}

void
MixerBridgeView::follow_editor_selection ()
{
	if (_following_editor_selection) {
		return;
	}

	_following_editor_selection = true;
	_selection.block_routes_changed (true);
	
	TrackSelection& s (PublicEditor::instance().get_selection().tracks);

	_selection.clear_routes ();

	for (TrackViewList::iterator i = s.begin(); i != s.end(); ++i) {
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*i);
		if (rtav) {
			MixerStrip* ms = strip_by_route (rtav->route());
			if (ms) {
				_selection.add (ms);
			}
		}
	}

	_following_editor_selection = false;
	_selection.block_routes_changed (false);
}

void
MixerBridgeView::set_route_targets_for_operation ()
{
	_route_targets.clear ();

	if (!_selection.empty()) {
		_route_targets = _selection.routes;
		return;
	}

	/* nothing selected ... try to get mixer strip at mouse */

	MixerStrip* ms = strip_under_pointer ();
	
	if (ms) {
		_route_targets.insert (ms);
	}
}

void
MixerBridgeView::toggle_midi_input_active (bool flip_others)
{
	boost::shared_ptr<RouteList> rl (new RouteList);
	bool onoff = false;

	set_route_targets_for_operation ();

	for (RouteUISelection::iterator r = _route_targets.begin(); r != _route_targets.end(); ++r) {
		boost::shared_ptr<MidiTrack> mt = (*r)->midi_track();

		if (mt) {
			rl->push_back ((*r)->route());
			onoff = !mt->input_active();
		}
	}
	
	_session->set_exclusive_input_active (rl, onoff, flip_others);
}

bool
MixerBridgeView::strip_button_release_event (GdkEventButton *ev, MixerStrip *strip)
{
    if (!_session) {
        return false;
    }
    
	if (ev->button == 1) {
        
        // primary modifier usecase
        if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier) ) {
            if (_selection.selected (strip) ){
                _selection.remove (strip);
            } else {
                _selection.add (strip);
            }
            
            return true;
        }
    
        // cesondary modifier usecase (multi-selection)
        if (Keyboard::modifier_state_equals (ev->state, Keyboard::RangeSelectModifier) )  {
            if (!_selection.selected((RouteUI*)strip)) {
                
                /* extend selection */
                vector<MixerStrip*> tmp;
                tmp.push_back (strip);
                
                // get ORDERED list of routes from the session
                // to acoomplish this - sort the list of routes as they are displayed
                // the same sorter is used to pack strips for mixer and meter
                SignalOrderRouteSorter sorter;
                boost::shared_ptr<RouteList> routes = _session->get_routes();
                RouteList sorted_routes(*routes);
                sorted_routes.sort(sorter);
                
                bool accumulate = false;
                bool passed_target = false;
                for (RouteList::iterator i = sorted_routes.begin(); i != sorted_routes.end(); ++i) {
                    
                    MixerStrip* mixer_strip = strip_by_route(*i);
                    
                    if (!mixer_strip) {
                        // we do not create MixerStrip for master bus
                        // it appears to be the last
                        // in the right case we won't hit the end
                        // because multi selection always happens between selected and slicked
                        continue;
                    }
                    
                    if (mixer_strip == strip) {
                        /* hit clicked strip, start accumulating till we hit the first
                         selected strip
                         */
                        if (accumulate) {
                            /* done */
                            break;
                        } else {
                            accumulate = true;
                            passed_target = true;
                        }
                        
                    } else if (_selection.selected ((RouteUI*)mixer_strip) ) {
                        /* hit selected strip. if currently accumulating others,
                         we're done. if not accumulating others, start doing so.
                         */
                        if (accumulate) {
                            
                            if (passed_target)
                                break;
                            
                        } else {
                            accumulate = true;
                        }
                    } else {
                        if (accumulate) {
                            tmp.push_back (mixer_strip);
                        }
                    }
                }
                
                _selection.block_routes_changed (true);
                for (vector<MixerStrip*>::iterator i = tmp.begin(); i != tmp.end(); ++i) {
                    _selection.add (*i);
                }
                _selection.block_routes_changed (false);
                _selection.RoutesChanged ();
            }
            
            return true;
        }
        
        // other cases
        _selection.set (strip);
        return true;
	}

	return false;
}


MixerStrip*
MixerBridgeView::strip_by_route (boost::shared_ptr<Route> route)
{
	std::map <boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.find (route);
	if (i != _strips.end ()) {
		return (*i).second;
	}

	return 0;
}

MixerStrip*
MixerBridgeView::strip_under_pointer ()
{
	int x, y;
	get_pointer (x, y);

	for (std::map<boost::shared_ptr<ARDOUR::Route>, MixerStrip*>::iterator i = _strips.begin(); i != _strips.end(); ++i) {
		int x1, x2, y1, y2;

		(*i).second->translate_coordinates (*this, 0, 0, x1, y1);
		x2 = x1 + (*i).second->get_width();
		y2 = y1 + (*i).second->get_height();

		if (x >= x1 && x < x2 && y >= y1 && y < y2) {
			return (*i).second;
		}
	}

	return 0;
}

void MixerBridgeView::parent_on_size_allocate (Gtk::Allocation& alloc)
{
	_mixer_strips_home.set_size_request (alloc.get_width (), -1);
}

void MixerBridgeView::delete_processors ()
{
        /* does nothing in Tracks */
}

void MixerBridgeView::select_none ()
{
        /* does nothing in Tracks */
}

void
MixerBridgeView::begin_strip_name_edit (MixerStrip::TabToStrip edit_next, const MixerStrip* cur_strip)
{
    std::vector<Gtk::Widget*> strips = _mixer_strips_home.get_children();
    if (edit_next == MixerStrip::TabToNext) {
        for (std::vector<Gtk::Widget*>::iterator it = strips.begin (); it != strips.end (); ++it) {
            if (*it == cur_strip) {
                if (++it != strips.end ()) {
                    MixerStrip* strip = dynamic_cast<MixerStrip*> (*it);
                    ensure_strip_is_visible (strip);
                    strip->begin_name_edit ();
                }
                break;
            }
        }
    } else { // MixerStrip::TabToPrev
        for (std::vector<Gtk::Widget*>::iterator it = strips.begin (); it != strips.end (); ++it) {
            if (*it == cur_strip) {
                if (it != strips.begin ()) {
                    --it;
                    MixerStrip* strip = dynamic_cast<MixerStrip*> (*it);
                    ensure_strip_is_visible (strip);
                    strip->begin_name_edit ();
                }
                break;
            }
        }
    }
}

int
MixerBridgeView::get_number_of_strip (const MixerStrip* cur_strip)
{
    // return number of strip in container '_mixer_strips_home'
    std::vector<Gtk::Widget*> strips = _mixer_strips_home.get_children();
    for (std::vector<Gtk::Widget*>::iterator it = strips.begin (); it != strips.end (); ++it) {
        if (*it == cur_strip) {
            return std::distance (strips.begin (), it);
        }
    }
    return -1;
}

int
MixerBridgeView::get_line_of_strip (const MixerStrip* cur_strip)
{
    // this method is actual just for Meterbridge
    const int STRIP_WIDTH = cur_strip->get_width ();
    int strips_per_line = _mixer_strips_home.get_width () / STRIP_WIDTH;
    int strip_number = get_number_of_strip (cur_strip);
    if (strip_number == -1) {
        return -1;
    }
    
    int strip_line_number = strip_number / strips_per_line;
    return strip_line_number;
}


void
MixerBridgeView::ensure_strip_is_visible (const MixerStrip* cur_mixer)
{
    Gtk::Adjustment* horizontal_adjustment = _scroll.get_hadjustment ();
    Gtk::Adjustment* vertical_adjustment = _scroll.get_vadjustment ();
    Gtk::Adjustment* using_adjustment;
    
    Gtk::Box* the_box = dynamic_cast <Gtk::Box*> (&_mixer_strips_home);
    WavesGrid* the_grid = dynamic_cast <WavesGrid*> (&_mixer_strips_home);
    
    double current_view_min_pos, current_view_max_pos;
    double strip_min_pos, strip_max_pos;
    
    if (the_box) { // Mixer
        const int STRIP_WIDTH = cur_mixer->get_width ();

        current_view_min_pos = horizontal_adjustment->get_value ();
        current_view_max_pos = current_view_min_pos + horizontal_adjustment->get_page_size ();
        
        strip_min_pos = get_number_of_strip (cur_mixer) * STRIP_WIDTH;
        strip_max_pos = strip_min_pos + STRIP_WIDTH;

        using_adjustment = horizontal_adjustment;
    } else if (the_grid) { //MeterBridge
        const int STRIP_HEIGHT = cur_mixer->get_height ();
        
        current_view_min_pos = vertical_adjustment->get_value ();
        current_view_max_pos = current_view_min_pos + vertical_adjustment->get_page_size ();
        
        strip_min_pos = get_line_of_strip (cur_mixer) * STRIP_HEIGHT;
        strip_max_pos = strip_min_pos + STRIP_HEIGHT;
      
        using_adjustment = vertical_adjustment;
    }
    else {
        return ;
    }
    
    if ( strip_min_pos >= current_view_min_pos &&
        strip_max_pos < current_view_max_pos ) {
        // already visible
        return;
    }
    
    double new_value = 0.0;
    if (strip_min_pos < current_view_min_pos) {
        // Strip is left (above) the current view
        new_value = strip_min_pos;
    } else {
        // Strip is right (below) the current view
        new_value = strip_max_pos - using_adjustment->get_page_size ();
    }
    using_adjustment->set_value (new_value);
}
