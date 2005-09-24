#ifndef __ardour_session_region_h__
#define __ardour_session_region_h__

#include <ardour/session.h>
#include <ardour/audioregion.h>

namespace ARDOUR {

template<class T> void Session::foreach_audio_region (T *obj, void (T::*func)(AudioRegion *))
{
	LockMonitor lm (region_lock, __LINE__, __FILE__);
	for (AudioRegionList::iterator i = audio_regions.begin(); i != audio_regions.end(); i++) {
		(obj->*func) ((*i).second);
	}
}

}

#endif /* __ardour_session_region_h__ */
