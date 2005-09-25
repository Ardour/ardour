#ifndef __ardour_gtk_route_selection_h__
#define __ardour_gtk_route_selection_h__

#include <list>

namespace ARDOUR {
	class Route;
}

struct RouteSelection : list<ARDOUR::Route*> {};

#endif /* __ardour_gtk_route_selection_h__ */
