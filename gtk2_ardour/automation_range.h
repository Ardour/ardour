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

#ifndef __ardour_gtk_automation_range_h__
#define __ardour_gtk_automation_range_h__

class TimeAxisView;

/** A rectangular range of an automation line, used to express a selected area.
 *
 *  x coordinates start/end are in AutomationList model coordinates.
 *  y coordinates are a expressed as a fraction of the AutomationTimeAxisView's height, where 0 is the
 *  bottom of the track, and 1 is the top.
 *
 *  This representation falls between the visible GUI control points and
 *  the back-end "actual" automation points, some of which may not be
 *  visible; it is not trivial to convert from one of these to the
 *  other, so the AutomationSelectable is a kind of "best and worst of
 *  both worlds".
 *
 *  It offers a zoom-independent representation of a selected area of automation.
 */
struct AutomationRange
{
	double start;
	double end;
	double low_fract;
	double high_fract;
	TimeAxisView* track; // ref would be better, but ARDOUR::SessionHandlePtr is non-assignable

	AutomationRange (double s, double e, double l, double h, TimeAxisView* atv)
		: start (s), end (e), low_fract (l), high_fract (h), track (atv) {}
};

#endif /* __ardour_gtk_automation_range_h__ */
