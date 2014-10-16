#include "ardour/sndfileimportable.h"
#include <sndfile.h>
#include <iostream>
#include <cstring>

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

	exists = true;
	int64_t ret = (uint32_t) binfo->time_reference_high;
	ret <<= 32;
	ret |= (uint32_t) binfo->time_reference_low;
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
