#ifndef __ardour_gtk_automation_selection_h__
#define __ardour_gtk_automation_selection_h__

#include <list>

namespace ARDOUR {
	class AutomationList;
}

struct AutomationSelection : list<ARDOUR::AutomationList*> {};

#endif /* __ardour_gtk_automation_selection_h__ */
