#ifndef __ardour_gtk_point_selection_h__
#define __ardour_gtk_point_selection_h__

#include <list>

#include "automation_selectable.h"

struct PointSelection : public std::list<AutomationSelectable> 
{
};

#endif /* __ardour_gtk_point_selection_h__ */
