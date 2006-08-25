#ifndef __ardour_session_region_h__
#define __ardour_session_region_h__

#include <ardour/session.h>
#include <ardour/audioregion.h>

namespace ARDOUR {

template<class T> void Session::foreach_audio_region (T *obj, void (T::*func)(boost::shared_ptr<AudioRegion>))
{
	Glib::Mutex::Lock lm (region_lock);
	for (AudioRegionList::iterator i = audio_regions.begin(); i != audio_regions.end(); i++) {
		(obj->*func) (i->second);
	}
}

}

#endif /* __ardour_session_region_h__ */
