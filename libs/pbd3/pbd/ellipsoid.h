/*
    Copyright (C) 1998-99 Paul Barton-Davis
 
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

#ifndef __qm_ellipsoid_h__
#define __qm_ellipsoid_h__

struct Arc
{
	int rect_x;
	int rect_y;
	int rect_h;
	int rect_w;
	int start_angle;
	int arc_angle;
	bool counter_clockwise;
};

class Ellipsoid
{
	int start_x;
	int end_x;
	int start_y;
	int end_y;
	static const unsigned int narcs;

 public:
        Arc arc[2];

	Ellipsoid () { 
		start_x = -1;
		end_x = -1;
	}

	bool ready() { return start_x != -1 && end_x != -1; }
	void set_start (int x, int y);
	void set_end (int x, int y);
	void compute ();

	void set_start_angle (int n, int which_arc = -1) {
		if (which_arc < 0) {
			arc[0].start_angle = n * 64; 
			arc[1].start_angle = n * 64; 
		} else if (which_arc < (int) narcs) {
			arc[which_arc].start_angle = n * 64;
		}
	}
	void set_arc_angle (int n, int which_arc = -1) { 
		if (which_arc < 0) {
			arc[0].arc_angle = n * 64;
			arc[1].arc_angle = n * 64;
		} else if (which_arc < (int) narcs) {
			arc[which_arc].arc_angle = n * 64;
		}
	}
};

#endif  // __qm_ellipsoid_h__
