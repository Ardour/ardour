#ifndef __ardour_gtk_gain_automation_time_axis_h__
#define __ardour_gtk_gain_automation_time_axis_h__

#include "canvas.h"
#include "automation_time_axis.h"

namespace ARDOUR {
	class Redirect;
	class Curve;
}

class GainAutomationTimeAxisView : public AutomationTimeAxisView
{
  public:
	GainAutomationTimeAxisView (ARDOUR::Session&,
				    ARDOUR::Route&,
				    PublicEditor&,
				    TimeAxisView& parent_axis,
				    ArdourCanvas::Canvas& canvas,
				    std::string name,
				    ARDOUR::Curve&);
	
	~GainAutomationTimeAxisView();

	void add_automation_event (ArdourCanvas::Item *item, GdkEvent *event, jack_nframes_t, double);
	
   private:
	ARDOUR::Curve& curve;

        void automation_changed ();
	void set_automation_state (ARDOUR::AutoState);
};

#endif /* __ardour_gtk_gain_automation_time_axis_h__ */
