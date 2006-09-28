#ifndef __ardour_gtk_pan_automation_time_axis_h__
#define __ardour_gtk_pan_automation_time_axis_h__

#include "canvas.h"
#include "automation_time_axis.h"

#include <gtkmm/comboboxtext.h>

namespace ARDOUR {
	class Redirect;
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
