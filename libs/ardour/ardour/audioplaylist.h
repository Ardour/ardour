/*
    Copyright (C) 2003 Paul Davis

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

#ifndef __ardour_audio_playlist_h__
#define __ardour_audio_playlist_h__

#include <vector>
#include <list>

#include "ardour/ardour.h"
#include "ardour/playlist.h"

namespace ARDOUR  {

class Session;
class Region;
class AudioRegion;
class Source;

namespace Properties {
	/* fake the type, since crossfades are handled by SequenceProperty which doesn't
	   care about such things.
	*/
	extern PBD::PropertyDescriptor<bool> crossfades;
}

class AudioPlaylist;

class CrossfadeListProperty : public PBD::SequenceProperty<std::list<boost::shared_ptr<Crossfade> > >
{
public:
	CrossfadeListProperty (AudioPlaylist &);

	void get_content_as_xml (boost::shared_ptr<Crossfade>, XMLNode &) const;
	boost::shared_ptr<Crossfade> get_content_from_xml (XMLNode const &) const;

private:
	CrossfadeListProperty* clone () const;
	CrossfadeListProperty* create () const;

	/* copy construction only by ourselves */
	CrossfadeListProperty (CrossfadeListProperty const & p);

	friend class AudioPlaylist;
	/* we live and die with our playlist, no lifetime management needed */
	AudioPlaylist& _playlist;
};


class AudioPlaylist : public ARDOUR::Playlist
{
public:
	typedef std::list<boost::shared_ptr<Crossfade> > Crossfades;
	static void make_property_quarks ();

	AudioPlaylist (Session&, const XMLNode&, bool hidden = false);
	AudioPlaylist (Session&, std::string name, bool hidden = false);
	AudioPlaylist (boost::shared_ptr<const AudioPlaylist>, std::string name, bool hidden = false);
	AudioPlaylist (boost::shared_ptr<const AudioPlaylist>, framepos_t start, framecnt_t cnt, std::string name, bool hidden = false);

	~AudioPlaylist ();

	void clear (bool with_signals=true);

	framecnt_t read (Sample *dst, Sample *mixdown, float *gain_buffer, framepos_t start, framecnt_t cnt, uint32_t chan_n=0);

	int set_state (const XMLNode&, int version);

	PBD::Signal1<void,boost::shared_ptr<Crossfade> >  NewCrossfade;

	void foreach_crossfade (boost::function<void (boost::shared_ptr<Crossfade>)>);
	void crossfades_at (framepos_t frame, Crossfades&);

	bool destroy_region (boost::shared_ptr<Region>);

	void update (const CrossfadeListProperty::ChangeRecord &);

	boost::shared_ptr<Crossfade> find_crossfade (const PBD::ID &) const;
	void get_equivalent_crossfades (boost::shared_ptr<Crossfade>, std::vector<boost::shared_ptr<Crossfade> > &);

protected:

	/* playlist "callbacks" */
	void notify_crossfade_added (boost::shared_ptr<Crossfade>);
	void flush_notifications (bool);

	void finalize_split_region (boost::shared_ptr<Region> orig, boost::shared_ptr<Region> left, boost::shared_ptr<Region> right);

	void refresh_dependents (boost::shared_ptr<Region> region);
	void check_dependents (boost::shared_ptr<Region> region, bool norefresh);
	void remove_dependents (boost::shared_ptr<Region> region);
	void copy_dependents (const std::vector<TwoRegions>&, Playlist*) const;

	void pre_combine (std::vector<boost::shared_ptr<Region> >&);
	void post_combine (std::vector<boost::shared_ptr<Region> >&, boost::shared_ptr<Region>);
	void pre_uncombine (std::vector<boost::shared_ptr<Region> >&, boost::shared_ptr<Region>);

private:
	CrossfadeListProperty _crossfades;
	Crossfades            _pending_xfade_adds;

	void crossfade_invalidated (boost::shared_ptr<Region>);
	XMLNode& state (bool full_state);
	void dump () const;

	bool region_changed (const PBD::PropertyChange&, boost::shared_ptr<Region>);
	void crossfade_changed (const PBD::PropertyChange&);
	void add_crossfade (boost::shared_ptr<Crossfade>);

	void source_offset_changed (boost::shared_ptr<AudioRegion> region);
};

} /* namespace ARDOUR */

#endif	/* __ardour_audio_playlist_h__ */


