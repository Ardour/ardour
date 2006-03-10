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

#include <ardour/sndfilesource.h>

#include "i18n.h"

using namespace ARDOUR;

SndFileSource::SndFileSource (const XMLNode& node)
	: ExternalSource (node)
{
	init (_name, true);
	SourceCreated (this); /* EMIT SIGNAL */
}

SndFileSource::SndFileSource (const string& idstr, bool build_peak)
	: ExternalSource(idstr, build_peak)
{
	init (idstr, build_peak);

	if (build_peak) {
		 SourceCreated (this); /* EMIT SIGNAL */
	}
}

void 
SndFileSource::init (const string& idstr, bool build_peak)
{
	string::size_type pos;
	string file;

	tmpbuf = 0;
	tmpbufsize = 0;
	sf = 0;

	_name = idstr;

	if ((pos = idstr.find_last_of (':')) == string::npos) {
		channel = 0;
		file = idstr;
	} else {
		channel = atoi (idstr.substr (pos+1).c_str());
		file = idstr.substr (0, pos);
	}

	/* although libsndfile says we don't need to set this,
	   valgrind and source code shows us that we do.
	*/

	memset (&_info, 0, sizeof(_info));

	/* note that we temporarily truncated _id at the colon */
	
	if ((sf = sf_open (file.c_str(), SFM_READ, &_info)) == 0) {
		char errbuf[256];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose(_("SndFileSource: cannot open file \"%1\" (%2)"), file, errbuf) << endmsg;
		throw failed_constructor();
	}

	if (channel >= _info.channels) {
		error << string_compose(_("SndFileSource: file only contains %1 channels; %2 is invalid as a channel number"), _info.channels, channel) << endmsg;
		sf_close (sf);
		sf = 0;
		throw failed_constructor();
	}

	_length = _info.frames;
	_path = file;

	if (build_peak) {
		if (initialize_peakfile (false, file)) {
			sf_close (sf);
			sf = 0;
			throw failed_constructor ();
		}
	}
}

SndFileSource::~SndFileSource ()

{
	GoingAway (this); /* EMIT SIGNAL */

	if (sf) {
		sf_close (sf);
	}

	if (tmpbuf) {
		delete [] tmpbuf;
	}
}

jack_nframes_t
SndFileSource::read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const
{
	int32_t nread;
	float *ptr;
	uint32_t real_cnt;

	if (sf_seek (sf, (off_t) start, SEEK_SET) < 0) {
		char errbuf[256];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose(_("SndFileSource: could not seek to frame %1 within %2 (%3)"), start, _name.substr (1), errbuf) << endmsg;
		return 0;
	}

	if (_info.channels == 1) {
		jack_nframes_t ret = sf_read_float (sf, dst, cnt);
		_read_data_count = cnt * sizeof(float);
		return ret;
	}

	real_cnt = cnt * _info.channels;

	{
		LockMonitor lm (_tmpbuf_lock, __LINE__, __FILE__);
		
		if (tmpbufsize < real_cnt) {
			
			if (tmpbuf) {
				delete [] tmpbuf;
			}
			tmpbufsize = real_cnt;
			tmpbuf = new float[tmpbufsize];
		}
		
		nread = sf_read_float (sf, tmpbuf, real_cnt);
		ptr = tmpbuf + channel;
		nread /= _info.channels;
		
		/* stride through the interleaved data */
		
		for (int32_t n = 0; n < nread; ++n) {
			dst[n] = *ptr;
			ptr += _info.channels;
		}
	}

	_read_data_count = cnt * sizeof(float);
		
	return nread;
}

