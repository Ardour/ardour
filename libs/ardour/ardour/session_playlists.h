#ifndef __ardour_session_playlists_h__
#define __ardour_session_playlists_h__

#include <set>
#include <vector>
#include <string>
#include <glibmm/thread.h>
#include <boost/shared_ptr.hpp>
#include <sigc++/trackable.h>

class XMLNode;

namespace ARDOUR {

class Playlist;
class Region;
class Source;
class Session;
	
class SessionPlaylists : public sigc::trackable
{
public:
	~SessionPlaylists ();
	
	boost::shared_ptr<Playlist> by_name (std::string name);
	uint32_t source_use_count (boost::shared_ptr<const Source> src) const;
	template<class T> void foreach (T *obj, void (T::*func)(boost::shared_ptr<Playlist>));
	void get (std::vector<boost::shared_ptr<Playlist> >&);
	void unassigned (std::list<boost::shared_ptr<Playlist> > & list);

private:
	friend class Session;
	
	bool add (boost::shared_ptr<Playlist>);
	void remove (boost::shared_ptr<Playlist>);
	void track (bool, boost::weak_ptr<Playlist>);
	
	uint32_t n_playlists() const;
	void find_equivalent_playlist_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >& result);
	void update_after_tempo_map_change ();
	void add_state (XMLNode *, bool);
	bool maybe_delete_unused (sigc::signal<int, boost::shared_ptr<Playlist> >);
	int load (Session &, const XMLNode&);
	int load_unused (Session &, const XMLNode&);
	boost::shared_ptr<Playlist> XMLPlaylistFactory (Session &, const XMLNode&);

	mutable Glib::Mutex lock;
	typedef std::set<boost::shared_ptr<Playlist> > List;
	List playlists;
	List unused_playlists;
};

}

#endif
