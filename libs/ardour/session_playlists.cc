/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <vector>

#include "ardour/debug.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/session_playlists.h"
#include "ardour/track.h"
#include "pbd/i18n.h"
#include "pbd/compose.h"
#include "pbd/xml++.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

SessionPlaylists::~SessionPlaylists ()
{
	DEBUG_TRACE (DEBUG::Destruction, "delete playlists\n");

	for (List::iterator i = playlists.begin(); i != playlists.end(); ) {
		SessionPlaylists::List::iterator tmp;

		tmp = i;
		++tmp;

		DEBUG_TRACE(DEBUG::Destruction, string_compose ("Dropping for used playlist %1 ; pre-ref = %2\n", (*i)->name(), (*i).use_count()));
		boost::shared_ptr<Playlist> keeper (*i);
		(*i)->drop_references ();

		i = tmp;
	}

	DEBUG_TRACE (DEBUG::Destruction, "delete unused playlists\n");
	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ) {
		List::iterator tmp;

		tmp = i;
		++tmp;

		DEBUG_TRACE(DEBUG::Destruction, string_compose ("Dropping for unused playlist %1 ; pre-ref = %2\n", (*i)->name(), (*i).use_count()));
		boost::shared_ptr<Playlist> keeper (*i);
		(*i)->drop_references ();

		i = tmp;
	}

	playlists.clear ();
	unused_playlists.clear ();
}

bool
SessionPlaylists::add (boost::shared_ptr<Playlist> playlist)
{
	Glib::Threads::Mutex::Lock lm (lock);

	bool const existing = find (playlists.begin(), playlists.end(), playlist) != playlists.end();

	if (!existing) {
		playlists.insert (playlists.begin(), playlist);
		playlist->InUse.connect_same_thread (*this, boost::bind (&SessionPlaylists::track, this, _1, boost::weak_ptr<Playlist>(playlist)));
		playlist->DropReferences.connect_same_thread (
			*this, boost::bind (&SessionPlaylists::remove_weak, this, boost::weak_ptr<Playlist> (playlist))
			);
	}

	return existing;
}

void
SessionPlaylists::remove_weak (boost::weak_ptr<Playlist> playlist)
{
	boost::shared_ptr<Playlist> p = playlist.lock ();
	if (p) {
		remove (p);
	}
}

void
SessionPlaylists::remove (boost::shared_ptr<Playlist> playlist)
{
	Glib::Threads::Mutex::Lock lm (lock);

	List::iterator i;

	i = find (playlists.begin(), playlists.end(), playlist);
	if (i != playlists.end()) {
		playlists.erase (i);
	}

	i = find (unused_playlists.begin(), unused_playlists.end(), playlist);
	if (i != unused_playlists.end()) {
		unused_playlists.erase (i);
	}
}

void
SessionPlaylists::update_tracking ()
{
	/* This is intended to be called during session-load, after loading
	 * playlists and re-assigning them to tracks (refcnt is up to date).
	 * Check playlist refcnt, move unused playlist to unused_playlists
	 * array (which may be the case when loading old sessions)
	 */
	for (List::iterator i = playlists.begin(); i != playlists.end(); ) {
		if ((*i)->hidden () || (*i)->used ()) {
			++i;
			continue;
		}

		warning << _("Session State: Unused playlist was listed as used.") << endmsg;

		assert (unused_playlists.find (*i) == unused_playlists.end());
		unused_playlists.insert (*i);

		List::iterator rm = i;
		++i;
		 playlists.erase (rm);
	}
}

void
SessionPlaylists::track (bool inuse, boost::weak_ptr<Playlist> wpl)
{
	boost::shared_ptr<Playlist> pl(wpl.lock());

	if (!pl) {
		return;
	}

	List::iterator x;

	if (pl->hidden()) {
		/* its not supposed to be visible */
		return;
	}

	{
		Glib::Threads::Mutex::Lock lm (lock);

		if (!inuse) {

			unused_playlists.insert (pl);

			if ((x = playlists.find (pl)) != playlists.end()) {
				playlists.erase (x);
			}


		} else {

			playlists.insert (pl);

			if ((x = unused_playlists.find (pl)) != unused_playlists.end()) {
				unused_playlists.erase (x);
			}
		}
	}
}

uint32_t
SessionPlaylists::n_playlists () const
{
	Glib::Threads::Mutex::Lock lm (lock);
	return playlists.size();
}

boost::shared_ptr<Playlist>
SessionPlaylists::by_name (string name)
{
	Glib::Threads::Mutex::Lock lm (lock);

	for (List::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}

	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}

	return boost::shared_ptr<Playlist>();
}

boost::shared_ptr<Playlist>
SessionPlaylists::by_id (const PBD::ID& id)
{
	Glib::Threads::Mutex::Lock lm (lock);

	for (List::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		if ((*i)->id() == id) {
			return* i;
		}
	}

	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		if ((*i)->id() == id) {
			return* i;
		}
	}

	return boost::shared_ptr<Playlist>();
}

void
SessionPlaylists::unassigned (std::list<boost::shared_ptr<Playlist> > & list)
{
	Glib::Threads::Mutex::Lock lm (lock);

	for (List::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		if (!(*i)->get_orig_track_id().to_s().compare ("0")) {
			list.push_back (*i);
		}
	}

	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		if (!(*i)->get_orig_track_id().to_s().compare ("0")) {
			list.push_back (*i);
		}
	}
}

void
SessionPlaylists::update_orig_2X (PBD::ID old_orig, PBD::ID new_orig)
{
	Glib::Threads::Mutex::Lock lm (lock);

	for (List::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		if ((*i)->get_orig_track_id() == old_orig) {
			(*i)->set_orig_track_id (new_orig);
		}
	}

	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		if ((*i)->get_orig_track_id() == old_orig) {
			(*i)->set_orig_track_id (new_orig);
		}
	}
}

void
SessionPlaylists::get (vector<boost::shared_ptr<Playlist> >& s) const
{
	Glib::Threads::Mutex::Lock lm (lock);

	for (List::const_iterator i = playlists.begin(); i != playlists.end(); ++i) {
		s.push_back (*i);
	}

	for (List::const_iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		s.push_back (*i);
	}
}

void
SessionPlaylists::destroy_region (boost::shared_ptr<Region> r)
{
	Glib::Threads::Mutex::Lock lm (lock);

	for (List::iterator i = playlists.begin(); i != playlists.end(); ++i) {
                (*i)->destroy_region (r);
	}

	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
                (*i)->destroy_region (r);
	}
}

void
SessionPlaylists::find_equivalent_playlist_regions (boost::shared_ptr<Region> region, vector<boost::shared_ptr<Region> >& result)
{
	for (List::iterator i = playlists.begin(); i != playlists.end(); ++i)
		(*i)->get_region_list_equivalent_regions (region, result);
}

/** Return the number of playlists (not regions) that contain @a src
 *  Important: this counts usage in both used and not-used playlists.
 */
uint32_t
SessionPlaylists::source_use_count (boost::shared_ptr<const Source> src) const
{
	uint32_t count = 0;

	/* XXXX this can go wildly wrong in the presence of circular references
	 * between compound regions.
	 */

	for (List::const_iterator p = playlists.begin(); p != playlists.end(); ++p) {
                if ((*p)->uses_source (src)) {
                        ++count;
                        break;
                }
	}

	for (List::const_iterator p = unused_playlists.begin(); p != unused_playlists.end(); ++p) {
                if ((*p)->uses_source (src)) {
                        ++count;
                        break;
                }
	}

	return count;
}

void
SessionPlaylists::sync_all_regions_with_regions ()
{
	Glib::Threads::Mutex::Lock lm (lock);

	for (List::const_iterator p = playlists.begin(); p != playlists.end(); ++p) {
                (*p)->sync_all_regions_with_regions ();
        }
}

void
SessionPlaylists::update_after_tempo_map_change ()
{
	for (List::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		(*i)->update_after_tempo_map_change ();
	}

	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		(*i)->update_after_tempo_map_change ();
	}
}

namespace {
struct id_compare
{
	bool operator()(const boost::shared_ptr<Playlist>& p1, const boost::shared_ptr<Playlist>& p2)
	{
		return p1->id () < p2->id ();
	}
};

typedef std::set<boost::shared_ptr<Playlist> > List;
typedef std::set<boost::shared_ptr<Playlist>, id_compare> IDSortedList;

static void
get_id_sorted_playlists (const List& playlists, IDSortedList& id_sorted_playlists)
{
	for (List::const_iterator i = playlists.begin(); i != playlists.end(); ++i) {
		id_sorted_playlists.insert(*i);
	}
}

} // anonymous namespace

void
SessionPlaylists::add_state (XMLNode* node, bool save_template, bool include_unused)
{
	XMLNode* child = node->add_child ("Playlists");

	IDSortedList id_sorted_playlists;
	get_id_sorted_playlists (playlists, id_sorted_playlists);

	for (IDSortedList::iterator i = id_sorted_playlists.begin (); i != id_sorted_playlists.end (); ++i) {
		if (!(*i)->hidden ()) {
			if (save_template) {
				child->add_child_nocopy ((*i)->get_template ());
			} else {
				child->add_child_nocopy ((*i)->get_state ());
			}
		}
	}

	if (!include_unused) {
		return;
	}

	child = node->add_child ("UnusedPlaylists");

	IDSortedList id_sorted_unused_playlists;
	get_id_sorted_playlists (unused_playlists, id_sorted_unused_playlists);

	for (IDSortedList::iterator i = id_sorted_unused_playlists.begin ();
	     i != id_sorted_unused_playlists.end (); ++i) {
		if (!(*i)->hidden()) {
			if (!(*i)->empty()) {
				if (save_template) {
					child->add_child_nocopy ((*i)->get_template());
				} else {
					child->add_child_nocopy ((*i)->get_state());
				}
			}
		}
	}
}

/** @return true for `stop cleanup', otherwise false */
bool
SessionPlaylists::maybe_delete_unused (boost::function<int(boost::shared_ptr<Playlist>)> ask)
{
	vector<boost::shared_ptr<Playlist> > playlists_tbd;

	bool delete_remaining = false;
	bool keep_remaining = false;

	for (List::iterator x = unused_playlists.begin(); x != unused_playlists.end(); ++x) {

		if (keep_remaining) {
			break;
		}

		if (delete_remaining) {
			playlists_tbd.push_back (*x);
			continue;
		}

		int status = ask (*x);

		switch (status) {
		case -1:
			// abort
			return true;

		case -2:
			// keep this and all later
			keep_remaining = true;
			break;

		case 2:
			// delete this and all later
			delete_remaining = true;

			/* fallthrough */
		case 1:
			// delete this
			playlists_tbd.push_back (*x);
			break;

		default:
			/* leave it alone */
			break;
		}
	}

	/* now delete any that were marked for deletion */

	for (vector<boost::shared_ptr<Playlist> >::iterator x = playlists_tbd.begin(); x != playlists_tbd.end(); ++x) {
		boost::shared_ptr<Playlist> keeper (*x);
		(*x)->drop_references ();
	}

	playlists_tbd.clear ();

	return false;
}

int
SessionPlaylists::load (Session& session, const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	boost::shared_ptr<Playlist> playlist;

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((playlist = XMLPlaylistFactory (session, **niter)) == 0) {
			error << _("Session: cannot create Playlist from XML description.") << endmsg;
			return -1;
		}
	}

	return 0;
}

int
SessionPlaylists::load_unused (Session& session, const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	boost::shared_ptr<Playlist> playlist;

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((playlist = XMLPlaylistFactory (session, **niter)) == 0) {
			error << _("Session: cannot create Playlist from XML description.") << endmsg;
			continue;
		}

		// now manually untrack it

		track (false, boost::weak_ptr<Playlist> (playlist));
	}

	return 0;
}

boost::shared_ptr<Playlist>
SessionPlaylists::XMLPlaylistFactory (Session& session, const XMLNode& node)
{
	try {
		return PlaylistFactory::create (session, node);
	}

	catch (failed_constructor& err) {
		return boost::shared_ptr<Playlist>();
	}
}

boost::shared_ptr<Crossfade>
SessionPlaylists::find_crossfade (const PBD::ID& id)
{
	Glib::Threads::Mutex::Lock lm (lock);

	boost::shared_ptr<Crossfade> c;

	for (List::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		c = (*i)->find_crossfade (id);
		if (c) {
			return c;
		}
	}

	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		c = (*i)->find_crossfade (id);
		if (c) {
			return c;
		}
	}

	return boost::shared_ptr<Crossfade> ();
}

uint32_t
SessionPlaylists::region_use_count (boost::shared_ptr<Region> region) const
{
	Glib::Threads::Mutex::Lock lm (lock);
        uint32_t cnt = 0;

	for (List::const_iterator i = playlists.begin(); i != playlists.end(); ++i) {
                cnt += (*i)->region_use_count (region);
	}

	for (List::const_iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
                cnt += (*i)->region_use_count (region);
	}

	return cnt;
}

vector<boost::shared_ptr<Playlist> >
SessionPlaylists::get_used () const
{
	vector<boost::shared_ptr<Playlist> > pl;

	Glib::Threads::Mutex::Lock lm (lock);

	for (List::const_iterator i = playlists.begin(); i != playlists.end(); ++i) {
		pl.push_back (*i);
	}

	return pl;
}

vector<boost::shared_ptr<Playlist> >
SessionPlaylists::get_unused () const
{
	vector<boost::shared_ptr<Playlist> > pl;

	Glib::Threads::Mutex::Lock lm (lock);

	for (List::const_iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		pl.push_back (*i);
	}

	return pl;
}

/** @return list of Playlists that are associated with a track */
vector<boost::shared_ptr<Playlist> >
SessionPlaylists::playlists_for_track (boost::shared_ptr<Track> tr) const
{
	vector<boost::shared_ptr<Playlist> > pl;
	get (pl);

	vector<boost::shared_ptr<Playlist> > pl_tr;

	for (vector<boost::shared_ptr<Playlist> >::iterator i = pl.begin(); i != pl.end(); ++i) {
		if ( ((*i)->get_orig_track_id() == tr->id()) ||
			(tr->playlist()->id() == (*i)->id())    ||
			((*i)->shared_with (tr->id())) )
		{
			pl_tr.push_back (*i);
		}
	}

	return pl_tr;
}

void
SessionPlaylists::foreach (boost::function<void(boost::shared_ptr<const Playlist>)> functor, bool incl_unused)
{
	Glib::Threads::Mutex::Lock lm (lock);
	for (List::iterator i = playlists.begin(); i != playlists.end(); i++) {
		if (!(*i)->hidden()) {
			functor (*i);
		}
	}
	if (!incl_unused) {
		return;
	}
	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); i++) {
		if (!(*i)->hidden()) {
			functor (*i);
		}
	}
}
