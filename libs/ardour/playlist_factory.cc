#include <ardour/session.h>

#include <ardour/playlist.h>
#include <ardour/audioplaylist.h>

#include <ardour/region_factory.h>
#include <ardour/region.h>
#include <ardour/audioregion.h>

#include "i18n.h"

using namespace ARDOUR;

Region*
ARDOUR::createRegion (const Region& region, jack_nframes_t start, 
		      jack_nframes_t length, std::string name, 
		      layer_t layer, Region::Flag flags)
{
	const AudioRegion* ar;
	
	if ((ar = dynamic_cast<const AudioRegion*>(&region)) != 0) {
		AudioRegion* ret;
		ret = new AudioRegion (*ar, start, length, name, layer, flags);
		return ret;
	} else {
		fatal << _("programming error: Playlist::createRegion called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
		return 0;
	}
}

Region*
ARDOUR::createRegion (const Region& region)
{
	const AudioRegion* ar;
	
	if ((ar = dynamic_cast<const AudioRegion*>(&region)) != 0) {
		return new AudioRegion (*ar);
	} else {
		fatal << _("programming error: Playlist::createRegion called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
		return 0;
	}
}

Region*
ARDOUR::createRegion (Session& session, XMLNode& node, bool yn)
{
	return session.XMLRegionFactory (node, yn);
}
	
Playlist*
Playlist::copyPlaylist (const Playlist& playlist, jack_nframes_t start, jack_nframes_t length,
			string name, bool result_is_hidden)
{
	const AudioPlaylist* apl;

	if ((apl = dynamic_cast<const AudioPlaylist*> (&playlist)) != 0) {
		return new AudioPlaylist (*apl, start, length, name, result_is_hidden);
	} else {
		fatal << _("programming error: Playlist::copyPlaylist called with unknown Playlist type")
		      << endmsg;
		/*NOTREACHED*/
		return 0;
	}
}
