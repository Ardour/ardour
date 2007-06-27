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

#ifndef __ardour_gtk_pan_automation_time_axis_h__
#define __ardour_gtk_pan_automation_time_axis_h__

#include "canvas.h"
#include "automation_time_axis.h"

#include <gtkmm/comboboxtext.h>

namespace ARDOUR {
	class IOProcessor;
}

class PanAutomationTimeAxisView : public AutomationTimeAxisView
{
	public:
		PanAutomationTimeAxisView (ARDOUR::Session&,
					   boost::shared_ptr<ARDOUR::Route>,
					   PublicEditor&,
					   TimeAxisView& parent_axis,
					   ArdourCanvas::Canvas& canvas,
					   std::string name);

		~PanAutomationTimeAxisView();

		void add_automation_event (ArdourCanvas::Item *item, GdkEvent *event, nframes_t, double);

		void clear_lines ();
		void add_line (AutomationLine&);
		void set_height (TimeAxisView::TrackHeight);

	protected:
		Gtk::ComboBoxText       multiline_selector;

	private:
		void automation_changed ();
		void set_automation_state (ARDOUR::AutoState);
};

#endif /* __ardour_gtk_pan_automation_time_axis_h__ */
