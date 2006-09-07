/*
    Copyright (C) 2006 Paul Davis 

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

    $Id$
*/

#ifndef __sndfile_source_h__ 
#define __sndfile_source_h__

#include <sndfile.h>

#include <ardour/audiofilesource.h>

namespace ARDOUR {

class SndFileSource : public AudioFileSource {
  public:
	/* constructor to be called for existing external-to-session files */

	SndFileSource (Session&, std::string path, Flag flags);

	/* constructor to be called for new in-session files */

	SndFileSource (Session&, std::string path, SampleFormat samp_format, HeaderFormat hdr_format, jack_nframes_t rate, 
		       Flag flags = AudioFileSource::Flag (AudioFileSource::Writable|
							   AudioFileSource::Removable|
							   AudioFileSource::RemovableIfEmpty|
							   AudioFileSource::CanRename));
		       
	/* constructor to be called for existing in-session files */
	
	SndFileSource (Session&, const XMLNode&);

	~SndFileSource ();

	float sample_rate () const;
	int update_header (jack_nframes_t when, struct tm&, time_t);
	int flush_header ();

	jack_nframes_t natural_position () const;

  protected:
	void set_header_timeline_position ();

	jack_nframes_t read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt) const;
	jack_nframes_t write_unlocked (Sample *dst, jack_nframes_t cnt);

	jack_nframes_t write_float (Sample* data, jack_nframes_t pos, jack_nframes_t cnt);

  private:
	SNDFILE *sf;
	SF_INFO _info;
	SF_BROADCAST_INFO *_broadcast_info;

	mutable float *interleave_buf;
	mutable jack_nframes_t interleave_bufsize;

	void init (const string &str);
	int open();
	void close();
	int setup_broadcast_info (jack_nframes_t when, struct tm&, time_t);
};

} // namespace ARDOUR

#endif /* __sndfile_source_h__ */

