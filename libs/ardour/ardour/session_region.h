#ifndef __ardour_session_region_h__
#define __ardour_session_region_h__

#include <ardour/session.h>
#include <ardour/audioregion.h>

namespace ARDOUR {

template<class T> void Session::foreach_region (T *obj, void (T::*func)(boost::shared_ptr<Region>))
{
	Glib::Mutex::Lock lm (region_lock);
	for (RegionList::iterator i = regions.begin(); i != regions.end(); i++) {
		(obj->*func) (i->second);
	}
}

} // namespace ARDOUR

#endif /* __ardour_session_region_h__ */
