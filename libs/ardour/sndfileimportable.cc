/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#include <sndfile.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"


#include "pbd/error.h"
#include "ardour/sndfileimportable.h"

#include <glibmm/convert.h>

using namespace ARDOUR;
using namespace std;

/* FIXME: this was copied from sndfilesource.cc, at some point these should be merged */
int64_t
SndFileImportableSource::get_timecode_info (SNDFILE* sf, SF_BROADCAST_INFO* binfo, bool& exists)
{
	if (sf_command (sf, SFC_GET_BROADCAST_INFO, binfo, sizeof (*binfo)) != SF_TRUE) {
		exists = false;
		return 0;
	}

	/* see http://tracker.ardour.org/view.php?id=6208
	 * 0xffffffff 0xfffc5680
	 * seems to be a bug in Presonus Capture (which generated the file)
	 *
	 * still since samplepos_t is a signed int, ignore files that could
	 * lead to negative timestamps for now.
	 */

	if (binfo->time_reference_high & 0x80000000) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "%x%08x", binfo->time_reference_high, binfo->time_reference_low);
		PBD::warning << "Invalid Timestamp " << tmp << endmsg;
		exists = false;
		return 0;
	}

	exists = true;
	/* libsndfile reads eactly 4 bytes for high and low, but
	 * uses "unsigned int" which may or may not be 32 bit little
	 * endian.
	 */
	int64_t ret = (uint32_t) (binfo->time_reference_high & 0x7fffffff);
	ret <<= 32;
	ret |= (uint32_t) (binfo->time_reference_low & 0xffffffff);

	assert(ret >= 0);
	return ret;
}

SndFileImportableSource::SndFileImportableSource (const string& path)
{
	int fd = g_open (path.c_str (), O_RDONLY, 0444);
	if (fd == -1) {
		throw failed_constructor ();
	}
	memset(&sf_info, 0 , sizeof(sf_info));
	in.reset (sf_open_fd (fd, SFM_READ, &sf_info, true), sf_close);
	if (!in) throw failed_constructor();

	SF_BROADCAST_INFO binfo;
	bool timecode_exists;

	memset (&binfo, 0, sizeof (binfo));
	timecode = get_timecode_info (in.get(), &binfo, timecode_exists);

	if (!timecode_exists) {
		timecode = 0;
	}
}

SndFileImportableSource::~SndFileImportableSource ()
{
}

samplecnt_t
SndFileImportableSource::read (Sample* buffer, samplecnt_t nframes)
{
	samplecnt_t per_channel = nframes / sf_info.channels;
	per_channel = sf_readf_float (in.get(), buffer, per_channel);
	return per_channel * sf_info.channels;
}

uint32_t
SndFileImportableSource::channels () const
{
	return sf_info.channels;
}

samplecnt_t
SndFileImportableSource::length () const
{
	return (samplecnt_t) sf_info.frames;
}

samplecnt_t
SndFileImportableSource::samplerate () const
{
	return sf_info.samplerate;
}

void
SndFileImportableSource::seek (samplepos_t /*pos*/)
{
	sf_seek (in.get(), 0, SEEK_SET);
}

samplepos_t
SndFileImportableSource::natural_position () const
{
	return (samplepos_t) timecode;
}

bool
SndFileImportableSource::clamped_at_unity () const
{
	int const type = sf_info.format & SF_FORMAT_TYPEMASK;
	int const sub = sf_info.format & SF_FORMAT_SUBMASK;
	/* XXX: this may not be the full list of formats that are unclamped */
	return (sub != SF_FORMAT_FLOAT && sub != SF_FORMAT_DOUBLE && type != SF_FORMAT_OGG);
}
