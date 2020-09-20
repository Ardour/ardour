/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2008 Doug McLain <doug@nostar.net>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __sndfile_source_h__
#define __sndfile_source_h__

#include <sndfile.h>

#include "ardour/audiofilesource.h"
#include "ardour/broadcast_info.h"
#include "ardour/progress.h"

namespace ARDOUR {

class LIBARDOUR_API SndFileSource : public AudioFileSource {
  public:
	/** Constructor to be called for existing external-to-session files */
	SndFileSource (Session&, const std::string& path, int chn, Flag flags);

	/* Constructor to be called for new in-session files */
	SndFileSource (Session&, const std::string& path, const std::string& origin,
	               SampleFormat samp_format, HeaderFormat hdr_format, samplecnt_t rate,
	               Flag flags = SndFileSource::default_writable_flags);

	/* Constructor to be called for recovering files being used for
	 * capture. They are in-session, they already exist, they should not
	 * be writable. They are an odd hybrid (from a constructor point of
	 * view) of the previous two constructors.
	 */
	SndFileSource (Session&, const std::string& path, int chn);

	/** Constructor to be called for existing in-session files during
	 * session loading
	 */
	SndFileSource (Session&, const XMLNode&);

	/** Constructor to losslessly compress existing source */
	SndFileSource (Session& s, const AudioFileSource& other, const std::string& path, bool use16bits = false, Progress* p = NULL);

	~SndFileSource ();

	float sample_rate () const;
	int update_header (samplepos_t when, struct tm&, time_t);
	int flush_header ();
	void flush ();

	bool one_of_several_channels () const;
	uint32_t channel_count () const { return _info.channels; }

	bool clamped_at_unity () const;

	static const Source::Flag default_writable_flags;

	static int get_soundfile_info (const std::string& path, SoundFileInfo& _info, std::string& error_msg);

  protected:
	void close ();

	void set_path (const std::string& p);
	void set_header_natural_position ();

	samplecnt_t read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const;
	samplecnt_t write_unlocked (Sample *dst, samplecnt_t cnt);
	samplecnt_t write_float (Sample* data, samplepos_t pos, samplecnt_t cnt);

  private:
	SNDFILE* _sndfile;
	SF_INFO _info;
	BroadcastInfo *_broadcast_info;

	void init_sndfile ();
	int open();
	int setup_broadcast_info (samplepos_t when, struct tm&, time_t);
	void file_closed ();

	void set_natural_position (timepos_t const &);
	samplecnt_t nondestructive_write_unlocked (Sample *dst, samplecnt_t cnt);
	PBD::ScopedConnection header_position_connection;
};

} // namespace ARDOUR

#endif /* __sndfile_source_h__ */

