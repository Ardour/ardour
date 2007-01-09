/*
    Copyright (C) 2006 Paul Davis 
	Written by Dave Robillard, 2006
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_midi_playlist_h__
#define __ardour_midi_playlist_h__

#include <vector>
#include <list>

#include <ardour/ardour.h>
#include <ardour/playlist.h>

namespace ARDOUR
{

class Session;
class Region;
class MidiRegion;
class Source;
class MidiRingBuffer;

class MidiPlaylist : public ARDOUR::Playlist
{
public:
	MidiPlaylist (Session&, const XMLNode&, bool hidden = false);
	MidiPlaylist (Session&, string name, bool hidden = false);
	MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other, string name, bool hidden = false);
	MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other, jack_nframes_t start, jack_nframes_t cnt,
	              string name, bool hidden = false);

	~MidiPlaylist ();

	nframes_t read (MidiRingBuffer& buf,
			jack_nframes_t start, jack_nframes_t cnt, uint32_t chan_n=0);

	int set_state (const XMLNode&);
	UndoAction get_memento() const;

	bool destroy_region (boost::shared_ptr<Region>);

protected:

	/* playlist "callbacks" */
	void flush_notifications ();

	void finalize_split_region (boost::shared_ptr<Region> original, boost::shared_ptr<Region> left, boost::shared_ptr<Region> right);
	
	void check_dependents (boost::shared_ptr<Region> region, bool norefresh);
	void refresh_dependents (boost::shared_ptr<Region> region);
	void remove_dependents (boost::shared_ptr<Region> region);

private:
	XMLNode& state (bool full_state);
	void dump () const;

	bool region_changed (Change, boost::shared_ptr<Region>);
};

} /* namespace ARDOUR */

#endif	/* __ardour_midi_playlist_h__ */


