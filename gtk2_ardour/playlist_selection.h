#ifndef __ardour_gtk_playlist_selection_h__
#define __ardour_gtk_playlist_selection_h__

#include <list>
#include <boost/shared_ptr.hpp>

namespace ARDOUR {
	class Playlist;
}

struct PlaylistSelection : list<boost::shared_ptr<ARDOUR::Playlist> > {};

#endif /* __ardour_gtk_playlist_selection_h__ */
