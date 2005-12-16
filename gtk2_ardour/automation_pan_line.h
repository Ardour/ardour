#ifndef __ardour_gtk_automation_pan_line_h__
#define __ardour_gtk_automation_pan_line_h__

#include <ardour/ardour.h>

#include "canvas.h"
#include "automation_line.h"

namespace ARDOUR {
	class Session;
}

class TimeAxisView;

class AutomationPanLine : public AutomationLine
{
  public:
	AutomationPanLine (ARDOUR::stringcr_t name, ARDOUR::Session&, TimeAxisView&, ArdourCanvas::Group& parent, ARDOUR::Curve&);
	
	void view_to_model_y (double&);
	void model_to_view_y (double&);

  private:
	ARDOUR::Session& session;
	vector<ArdourCanvas::Item*> lines;
};


#endif /* __ardour_gtk_automation_pan_line_h__ */


