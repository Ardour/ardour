#ifndef __ardour_gtk_redirect_selection_h__
#define __ardour_gtk_redirect_selection_h__

#include <list>
#include <boost/shared_ptr.hpp>

namespace ARDOUR {
	class Redirect;
}

struct RedirectSelection : list<boost::shared_ptr<ARDOUR::Redirect> > {};

#endif /* __ardour_gtk_redirect_selection_h__ */
