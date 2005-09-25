#ifndef __ardour_gtk_playlist_selection_h__
#define __ardour_gtk_playlist_selection_h__

#include <list>

namespace ARDOUR {
	class Playlist;
}

struct PlaylistSelection : list<ARDOUR::Playlist*> {};

#endif /* __ardour_gtk_playlist_selection_h__ */
