/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __ardour_audio_playlist_source_h__
#define __ardour_audio_playlist_source_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/ardour.h"
#include "ardour/audiosource.h"
#include "ardour/playlist_source.h"

namespace ARDOUR {

class AudioPlaylist;

class LIBARDOUR_API AudioPlaylistSource : public PlaylistSource, public AudioSource  {
public:
	virtual ~AudioPlaylistSource ();

	bool empty() const;
	std::string peak_path (std::string audio_path);
	uint32_t   n_channels() const;
	bool clamped_at_unity () const { return false; }

	framecnt_t read_unlocked (Sample *dst, framepos_t start, framecnt_t cnt) const;
	framecnt_t write_unlocked (Sample *src, framecnt_t cnt);

	float sample_rate () const;
	int setup_peakfile ();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool can_truncate_peaks() const { return false; }
	bool can_be_analysed() const    { return _length > 0; }

protected:
	friend class SourceFactory;

	AudioPlaylistSource (Session&, const PBD::ID& orig, const std::string& name, boost::shared_ptr<AudioPlaylist>, uint32_t chn,
	                     frameoffset_t begin, framecnt_t len, Source::Flag flags);
	AudioPlaylistSource (Session&, const XMLNode&);


private:
	uint32_t    _playlist_channel;
	std::string _peak_path;

	int set_state (const XMLNode&, int version, bool with_descendants);
};

} /* namespace */

#endif /* __ardour_audio_playlist_source_h__ */
