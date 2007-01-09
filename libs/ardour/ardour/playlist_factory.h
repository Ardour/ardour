#ifndef __ardour_playlist_factory_h__
#define __ardour_playlist_factory_h__

#include <ardour/playlist.h>

class XMLNode;

namespace ARDOUR {

class Session;

class PlaylistFactory {

  public:
	static sigc::signal<void,boost::shared_ptr<Playlist> > PlaylistCreated;

	static boost::shared_ptr<Playlist> create (Session&, const XMLNode&, bool hidden = false);
	static boost::shared_ptr<Playlist> create (DataType type, Session&, string name, bool hidden = false);
	static boost::shared_ptr<Playlist> create (boost::shared_ptr<const Playlist>, string name, bool hidden = false);
	static boost::shared_ptr<Playlist> create (boost::shared_ptr<const Playlist>, nframes_t start, nframes_t cnt, string name, bool hidden = false);
};

}

#endif /* __ardour_playlist_factory_h__  */
