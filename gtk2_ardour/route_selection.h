#ifndef __ardour_gtk_route_selection_h__
#define __ardour_gtk_route_selection_h__


#include <boost/shared_ptr.hpp>
#include <list>

namespace ARDOUR {
	class Route;
}

struct RouteSelection : std::list<boost::shared_ptr<ARDOUR::Route> > {};

#endif /* __ardour_gtk_route_selection_h__ */
