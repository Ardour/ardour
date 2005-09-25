#ifndef __ardour_gtk_redirect_selection_h__
#define __ardour_gtk_redirect_selection_h__

#include <list>

namespace ARDOUR {
	class Redirect;
}

struct RedirectSelection : list<ARDOUR::Redirect*> {};

#endif /* __ardour_gtk_redirect_selection_h__ */
