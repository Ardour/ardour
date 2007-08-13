#ifndef __ardour_gtk_boolean_automation_line_h__
#define __ardour_gtk_boolean_automation_line_h__

#include "automation_line.h"

class BooleanAutomationLine : public AutomationLine
{
  public:
        BooleanAutomationLine (const string & name, TimeAxisView&, ArdourCanvas::Group&, ARDOUR::AutomationList&);
	virtual ~BooleanAutomationLine ();

	void view_to_model_y (double&);
	void model_to_view_y (double&);

  protected:
	void add_model_point (AutomationLine::ALPoints& tmp_points, double frame, double yfract);
};

#endif /* __ardour_gtk_boolean_automation_line_h__ */
