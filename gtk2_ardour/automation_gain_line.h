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

#ifndef __ardour_gtk_automation_gain_line_h__
#define __ardour_gtk_automation_gain_line_h__

#include <ardour/ardour.h>

#include "canvas.h"
#include "automation_line.h"

class TimeAxisView;

class AutomationGainLine : public AutomationLine
{
  public:
	AutomationGainLine (const string & name, TimeAxisView&, ArdourCanvas::Group& parent, ARDOUR::AutomationList&);
	
	void view_to_model_y (double&);
	void model_to_view_y (double&);
};


#endif /* __ardour_gtk_automation_gain_line_h__ */


