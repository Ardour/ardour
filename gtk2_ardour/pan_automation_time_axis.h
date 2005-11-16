#ifndef __ardour_gtk_pan_automation_time_axis_h__
#define __ardour_gtk_pan_automation_time_axis_h__

#include "canvas.h"
#include "automation_time_axis.h"

namespace ARDOUR {
	class Redirect;
}

class PanAutomationTimeAxisView : public AutomationTimeAxisView
{
  public:
	PanAutomationTimeAxisView (ARDOUR::Session&,
				   ARDOUR::Route&,
				   PublicEditor&,
				   TimeAxisView& parent_axis,
				   ArdourCanvas::Canvas& canvas,
				   std::string name);

	~PanAutomationTimeAxisView();

	void add_automation_event (ArdourCanvas::Item *item, GdkEvent *event, jack_nframes_t, double);
	
   private:
        void automation_changed ();
	void set_automation_state (ARDOUR::AutoState);
};

#endif /* __ardour_gtk_pan_automation_time_axis_h__ */
