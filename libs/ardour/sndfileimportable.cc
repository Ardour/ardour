#include "ardour/sndfileimportable.h"
#include <sndfile.h>
#include <iostream>

using namespace ARDOUR;
using namespace std;

SndFileImportableSource::SndFileImportableSource (const string& path)
	: in (sf_open (path.c_str(), SFM_READ, &sf_info), sf_close)
{
	if (!in) throw failed_constructor();
}

SndFileImportableSource::~SndFileImportableSource ()
{
}

nframes_t
SndFileImportableSource::read (Sample* buffer, nframes_t nframes) 
{
	nframes_t per_channel = nframes / sf_info.channels;
	per_channel = sf_readf_float (in.get(), buffer, per_channel);
	return per_channel * sf_info.channels;
}

uint
SndFileImportableSource::channels () const 
{
	return sf_info.channels;
}

nframes_t
SndFileImportableSource::length () const 
{
	return sf_info.frames;
}

nframes_t
SndFileImportableSource::samplerate() const
{
	return sf_info.samplerate;
}

void
SndFileImportableSource::seek (nframes_t pos)
{
	sf_seek (in.get(), 0, SEEK_SET);
}
