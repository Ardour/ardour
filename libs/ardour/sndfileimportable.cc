/*
    Copyright (C) 2000,2015 Paul Davis

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <sndfile.h>
#include <iostream>
#include <cstring>

#include "pbd/error.h"
#include "ardour/sndfileimportable.h"

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
	 * still since framepos_t is a signed int, ignore files that could
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
	memset(&sf_info, 0 , sizeof(sf_info));
	in.reset( sf_open(path.c_str(), SFM_READ, &sf_info), sf_close);
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

framecnt_t
SndFileImportableSource::read (Sample* buffer, framecnt_t nframes)
{
	framecnt_t per_channel = nframes / sf_info.channels;
	per_channel = sf_readf_float (in.get(), buffer, per_channel);
	return per_channel * sf_info.channels;
}

uint32_t
SndFileImportableSource::channels () const
{
	return sf_info.channels;
}

framecnt_t
SndFileImportableSource::length () const
{
	return (framecnt_t) sf_info.frames;
}

framecnt_t
SndFileImportableSource::samplerate () const
{
	return sf_info.samplerate;
}

void
SndFileImportableSource::seek (framepos_t /*pos*/)
{
	sf_seek (in.get(), 0, SEEK_SET);
}

framepos_t
SndFileImportableSource::natural_position () const
{
	return (framepos_t) timecode;
}

bool
SndFileImportableSource::clamped_at_unity () const
{
	int const type = sf_info.format & SF_FORMAT_TYPEMASK;
	int const sub = sf_info.format & SF_FORMAT_SUBMASK;
	/* XXX: this may not be the full list of formats that are unclamped */
	return (sub != SF_FORMAT_FLOAT && sub != SF_FORMAT_DOUBLE && type != SF_FORMAT_OGG);
}
