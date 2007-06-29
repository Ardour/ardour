/*
    Copyright (C) 2000 Paul Davis 

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

#ifndef __ardour_gtk_processor_automation_line_h__
#define __ardour_gtk_processor_automation_line_h__

#include <ardour/ardour.h>

#include "automation_line.h"

namespace ARDOUR {
	class Processor;
}

class TimeAxisView;

class ProcessorAutomationLine : public AutomationLine
{
  public:
	ProcessorAutomationLine (const string & name, ARDOUR::Processor&,
			TimeAxisView&, ArdourCanvas::Group& parent, boost::shared_ptr<ARDOUR::AutomationList>);
	
	ARDOUR::Processor& processor() const { return _processor; }

	string get_verbose_cursor_string (float);

  private:
	ARDOUR::Processor& _processor;

	float _upper;
	float _lower;
	float _range;

	void view_to_model_y (double&);
	void model_to_view_y (double&);
};

#endif /* __ardour_gtk_region_gain_line_h__ */
