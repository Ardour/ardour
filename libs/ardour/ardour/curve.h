/*
    Copyright (C) 2001-2003 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_curve_h__
#define __ardour_curve_h__

#include <sys/types.h>
#include <sigc++/signal.h>
#include <glibmm/thread.h>
#include <pbd/undo.h>
#include <list>
#include <algorithm>
#include <ardour/automation_event.h>

namespace ARDOUR {

struct CurvePoint : public ControlEvent 
{
    double coeff[4];

    CurvePoint (double w, double v) 
	    : ControlEvent (w, v) {

	    coeff[0] = coeff[1] = coeff[2] = coeff[3] = 0.0;
    }

    ~CurvePoint() {}
};

class Curve : public AutomationList
{
  public:
	Curve (double min_yval, double max_yval, double defaultvalue, bool nostate = false);
	~Curve ();
	Curve (const Curve& other);
	Curve (const Curve& other, double start, double end);

	bool rt_safe_get_vector (double x0, double x1, float *arg, int32_t veclen);
	void get_vector (double x0, double x1, float *arg, int32_t veclen);

	AutomationEventList::iterator closest_control_point_before (double xval);
	AutomationEventList::iterator closest_control_point_after (double xval);

	void solve ();
		
  protected:
	ControlEvent* point_factory (double,double) const;
	ControlEvent* point_factory (const ControlEvent&) const;

	Change   restore_state (StateManager::State&);

  private:
	AutomationList::iterator last_bound;

	double unlocked_eval (double where);
	double multipoint_eval (double x);

	void _get_vector (double x0, double x1, float *arg, int32_t veclen);

};

}; /* namespace ARDOUR */

extern "C" {
	void curve_get_vector_from_c (void *arg, double, double, float*, int32_t);
}

#endif /* __ardour_curve_h__ */
