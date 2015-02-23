/*
    Copyright (C) 2000-2007 Paul Davis

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

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cmath>

#include <gtkmm/window.h>
#include <pangomm/layout.h>

#include "pbd/controllable.h"
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/persistent_tooltip.h"

#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"

#include "public_editor.h"
#include "selection.h"
#include "ardour_ui.h"
#include "global_signals.h"
#include "mono_panner.h"
#include "mono_panner_editor.h"
#include "rgb_macros.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR_UI_UTILS;

Gdk::Cursor* MonoPanner::__touch_cursor;
double MonoPanner::DEFAULT_VALUE = 0.5;

MonoPanner::MonoPanner (boost::shared_ptr<ARDOUR::PannerShell> p)
	: PannerInterface (p->panner())
	, _panner_shell (p)
	, position_control (_panner->pannable()->pan_azimuth_control)
	, drag_start_y (0)
	, last_drag_y (0)
	, accumulated_delta (0)
	, detented (false)
	, position_binder (position_control)
	, _dragging (false)
{
    init ();
}

MonoPanner::MonoPanner (boost::shared_ptr<ARDOUR::Route> route, boost::shared_ptr<ARDOUR::PannerShell> p)
: PannerInterface (p->panner())
, _route (route)
, _panner_shell (p)
, position_control (_panner->pannable()->pan_azimuth_control)
, drag_start_y (0)
, last_drag_y (0)
, accumulated_delta (0)
, detented (false)
, position_binder (position_control)
, _dragging (false)
{
    init ();
}

MonoPanner::~MonoPanner ()
{
	
}

void
MonoPanner::init ()
{
    if (_knob_image[0] == 0) {
        for (size_t i=0; i < (sizeof(_knob_image)/sizeof(_knob_image[0])); i++) {
            _knob_image[i] = load_pixbuf (_knob_image_files[i]);
        }
    }
    
    if (__touch_cursor == 0) {
        __touch_cursor = new Gdk::Cursor (Gdk::Display::get_default(),
                                          ::get_icon ("panner_touch_cursor"),
                                          12,
                                          12);
    }
    
    position_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&MonoPanner::value_change, this), gui_context());
    
    _panner_shell->Changed.connect (panshell_connections, invalidator (*this), boost::bind (&MonoPanner::bypass_handler, this), gui_context());
    _panner_shell->PannableChanged.connect (panshell_connections, invalidator (*this), boost::bind (&MonoPanner::pannable_handler, this), gui_context());
    
    set_tooltip ();
}

void
MonoPanner::set_tooltip ()
{
	if (_panner_shell->bypassed()) {
		_tooltip.set_tip (_("bypassed"));
		return;
	}
	double pos = position_control->get_value(); // 0..1

	/* We show the position of the center of the image relative to the left & right.
		 This is expressed as a pair of percentage values that ranges from (100,0)
		 (hard left) through (50,50) (hard center) to (0,100) (hard right).

		 This is pretty wierd, but its the way audio engineers expect it. Just remember that
		 the center of the USA isn't Kansas, its (50LA, 50NY) and it will all make sense.
		 */

	char buf[64];
	snprintf (buf, sizeof (buf), _("L:%3d R:%3d"),
			(int) rint (100.0 * (1.0 - pos)),
			(int) rint (100.0 * pos));
	_tooltip.set_tip (buf);
}

void
MonoPanner::render (cairo_t* cr, cairo_rectangle_t*)
{
	unsigned pos = (unsigned)(rint (100.0 * position_control->get_value ())); /* 0..100 */

	int x = (int)((get_width() - _knob_image[pos]->get_width())/2.0);
	int y = (int)((get_height() - _knob_image[pos]->get_height())/2.0);

	cairo_rectangle (cr, x, y, _knob_image[pos]->get_width(), _knob_image[pos]->get_height());

	gdk_cairo_set_source_pixbuf (cr, _knob_image[pos]->gobj(), x, y);
	cairo_fill (cr);
}

bool
MonoPanner::on_button_press_event (GdkEventButton* ev)
{
	if (PannerInterface::on_button_press_event (ev)) {
		return true;
	}

	if (_panner_shell->bypassed()) {
		return false;
	}

	get_window()->set_cursor (*__touch_cursor);

	drag_start_y = ev->y;
	last_drag_y = ev->y;

	_dragging = false;
	_tooltip.target_stop_drag ();
	accumulated_delta = 0;
	detented = false;

	/* Let the binding proxies get first crack at the press event
	*/

	if (ev->y < 20) {
		if (position_binder.button_press_handler (ev)) {
			return true;
		}
	}

	if (ev->button != 1) {
		return false;
	}

    int alt_modifier;
#ifdef __APPLE__
    alt_modifier = Keyboard::Level4Modifier; /* Alt */
#else
    /* Anything except OS X */
    alt_modifier = Keyboard::SecondaryModifier; /* Alt */
#endif
    
	if (ev->type == GDK_BUTTON_PRESS) {
        if (Keyboard::modifier_state_contains (ev->state, alt_modifier)) {
            
            // reset panner to default value
            if (_route && _route->main_outs()->panner() == _panner) {
                
                // determine if we deal with selection
                PublicEditor& editor = ARDOUR_UI::instance()->the_editor();
                Selection& selection = editor.get_selection();
                TimeAxisView* tv = editor.get_route_view_by_route_id (_route->id() );
                
                if (tv && selection.selected(tv) && selection.tracks.size() > 1 ) {
                    boost::shared_ptr<ARDOUR::RouteList> routes = get_selected_routes ();
                    
                    ARDOUR::RouteList::const_iterator iter = routes->begin();
                    for (; iter != routes->end(); ++iter) {
                        
                        // in multi out mode mono track will have no panner
                        if ((*iter)->panner() ) {
                            boost::shared_ptr<ARDOUR::AutomationControl> control;
                            control = (*iter)->panner()->pannable()->pan_azimuth_control;
                            control->set_value (DEFAULT_VALUE);
                        }
                    }
                } else {
                    
                    // in multi out mode mono track will have no panner
                    if (_route->panner() ) {
                        boost::shared_ptr<ARDOUR::AutomationControl> control;
                        control = _route->panner()->pannable()->pan_azimuth_control;
                        control->set_value (DEFAULT_VALUE);
                    }
                }

            } else {
                position_control->set_value (DEFAULT_VALUE);
            }
            
            _dragging = false;
            _tooltip.target_stop_drag ();
        } else {
            if (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
                /* handled by button release */
                return true;
            }

            _dragging = true;
            _tooltip.target_start_drag ();
            StartGesture ();
        }
	}

	return true;
}

bool
MonoPanner::on_button_release_event (GdkEventButton* ev)
{
	if (PannerInterface::on_button_release_event (ev)) {
		return true;
	}

	if (ev->button != 1) {
		return false;
	}

	if (_panner_shell->bypassed()) {
		return false;
	}

	get_window()->set_cursor();

	_dragging = false;
	_tooltip.target_stop_drag ();
	accumulated_delta = 0;
	detented = false;
    
	StopGesture ();

	return true;
}

bool
MonoPanner::on_scroll_event (GdkEventScroll* ev)
{
	double one_degree = 1.0/180.0; // one degree as a number from 0..1, since 180 degrees is the full L/R axis
	double pv = position_control->get_value(); // 0..1.0 ; 0 = left
	double step;

	if (_panner_shell->bypassed()) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
		step = one_degree;
	} else {
		step = one_degree * 5.0;
	}

	switch (ev->direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_LEFT:
			pv -= step;
			position_control->set_value (pv);
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_RIGHT:
			pv += step;
			position_control->set_value (pv);
			break;
	}

	return true;
}

bool
MonoPanner::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_panner_shell->bypassed()) {
		_dragging = false;
	}
	if (!_dragging) {
		return false;
	}

	int w = get_width();
	double delta = (last_drag_y - ev->y) / (double) w;

    adjust_pan_value_by (delta);

	last_drag_y = ev->y;
	return true;
}

boost::shared_ptr<ARDOUR::RouteList>
MonoPanner::get_selected_routes ()
{
    Selection& selection = ARDOUR_UI::instance()->the_editor().get_selection();
    
    boost::shared_ptr<ARDOUR::RouteList> routes (new ARDOUR::RouteList);
    
    TrackViewList track_list = selection.tracks;
    TrackViewList::const_iterator iter = track_list.begin ();
    for (; iter != track_list.end(); ++iter) {
        RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*iter);
        
        if (rtv) {
            routes->push_back(rtv->route() );
        }
    }
    
    return routes;

}

void
MonoPanner::adjust_pan_value_by (double delta)
{
    // first, calculate correct increment delta value
    double update_by_delta = 0;
    
    /* create a detent close to the center */
    if (!detented && ARDOUR::Panner::equivalent (position_control->get_value(), DEFAULT_VALUE)) {
        detented = true;
    }
    
    if (detented) {
        accumulated_delta += delta;
        
        /* have we pulled far enough to escape ? */
        
        if (fabs (accumulated_delta) >= 0.025) {
            update_by_delta = accumulated_delta;
            detented = false;
            accumulated_delta = 0;
        }
    } else {
        update_by_delta = delta;
    }
    
    // we've reached the bottom or the top, so don't move in this direction any more
    double original_val = position_control->get_value();
    if ( (update_by_delta > 0 && original_val >= 1 ) ||
        (update_by_delta < 0 && original_val <= 0 ) ) {
        return;
    }
    
    // make sure that calculated delta won't exceed the value we need to reach the top
    // or make us to go lower then the the bottom
    double result_value = original_val + update_by_delta;
    if (result_value < 0 ) {
        update_by_delta = update_by_delta - result_value;
    } else if (result_value > 1) {
        update_by_delta = update_by_delta - (result_value - 1);
    }
    
    // apply change to the associated route if we have it
    if (_route && _route->main_outs()->panner() == _panner) {
        
        // determine if we deal with selection
        PublicEditor& editor = ARDOUR_UI::instance()->the_editor();
        Selection& selection = editor.get_selection();
        TimeAxisView* tv = editor.get_route_view_by_route_id (_route->id() );
        
        if (tv && selection.selected(tv) && selection.tracks.size() > 1 ) {
            boost::shared_ptr<ARDOUR::RouteList> routes = get_selected_routes ();
            
            // apply the change
            ARDOUR::RouteList::const_iterator iter = routes->begin();
            for (; iter != routes->end(); ++iter) {
                
                // in multi out mode mono track will have no panner
                if ((*iter)->panner() ) {
                    boost::shared_ptr<ARDOUR::AutomationControl> control;
                    control = (*iter)->panner()->pannable()->pan_azimuth_control;
                
                    double pv = control->get_value(); // 0..1.0 ; 0 = left
                    control->set_value (pv + update_by_delta);
                }
            }
            
        } else {
            
            // in multi out mode mono track will have no panner
            if (_route->panner() ) {
                boost::shared_ptr<ARDOUR::AutomationControl> control;
                control = _route->panner()->pannable()->pan_azimuth_control;
    
                double pv = control->get_value(); // 0..1.0 ; 0 = left
                control->set_value (pv + update_by_delta);
            }
        }
        
    } else { // apply change to the panner directly
        double pv = position_control->get_value(); // 0..1.0 ; 0 = left
        position_control->set_value (pv + update_by_delta);
    }
}

bool
MonoPanner::on_key_press_event (GdkEventKey* ev)
{
	double one_degree = 1.0/180.0;
	double pv = position_control->get_value(); // 0..1.0 ; 0 = left
	double step;

	if (_panner_shell->bypassed()) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
		step = one_degree;
	} else {
		step = one_degree * 5.0;
	}

	switch (ev->keyval) {
		case GDK_Left:
			pv -= step;
			position_control->set_value (pv);
			break;
		case GDK_Right:
			pv += step;
			position_control->set_value (pv);
			break;
		case GDK_0:
		case GDK_KP_0:
			position_control->set_value (0.0);
			break;
		default:
			return false;
	}

	return true;
}

void
MonoPanner::bypass_handler ()
{
	queue_draw ();
}

void
MonoPanner::pannable_handler ()
{
	panvalue_connections.drop_connections();
	position_control = _panner->pannable()->pan_azimuth_control;
	position_binder.set_controllable(position_control);
	position_control->Changed.connect (panvalue_connections, invalidator(*this), boost::bind (&MonoPanner::value_change, this), gui_context());
	queue_draw ();
}

PannerEditor*
MonoPanner::editor ()
{
	return new MonoPannerEditor (this);
}
