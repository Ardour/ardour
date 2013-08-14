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
#include <fstream>
#include <cfloat>
#include <unistd.h>

#include "ardour/automation_list.h"
#include "evoral/Curve.hpp"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

int
curvetest (string filename)
{
	ifstream in (filename.c_str());
	stringstream line;
	//Evoral::Parameter param(GainAutomation, -1.0, +1.0, 0.0);
	Evoral::Parameter param(GainAutomation);
	AutomationList al (param);
	double minx = DBL_MAX;
	double maxx = DBL_MIN;

	while (in) {
		double x, y;

		in >> x;
		in >> y;

		if (!in) {
			break;
		}

		if (x < minx) {
			minx = x;
		}

		if (x > maxx) {
			maxx = x;
		}

		al.add (x, y);
	}


	float foo[1024];

	al.curve().get_vector (minx, maxx, foo, 1024);

	for (int i = 0; i < 1024; ++i) {
	        cout << minx + (((double) i / 1024.0) * (maxx - minx)) << ' ' << foo[i] << endl;
	}

	return 0;
}
