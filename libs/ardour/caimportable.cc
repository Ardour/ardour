/*
    Copyright (C) 2012 Paul Davis 

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

#include "ardour/caimportable.h"
#include <sndfile.h>
#include "pbd/error.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

CAImportableSource::CAImportableSource (const string& path)
{
	try {
		af.Open (path.c_str());

		CAStreamBasicDescription file_format (af.GetFileDataFormat());
		CAStreamBasicDescription client_format (file_format);

		/* set canonial form (PCM, native float packed, 32 bit, with the correct number of channels
		   and interleaved (since we plan to deinterleave ourselves)
		*/

		client_format.SetCanonical(client_format.NumberChannels(), true);
		af.SetClientFormat (client_format);

	} catch (CAXException& cax) {
                //Don't report an error here since there is one higher up in import.
                //Since libsndfile gets tried second, any failures here may show as
                //invalid errors in the Error log.
		throw failed_constructor ();
	}

}

CAImportableSource::~CAImportableSource ()
{
}

framecnt_t
CAImportableSource::read (Sample* buffer, framecnt_t nframes)
{
	framecnt_t nread = 0;
	AudioBufferList abl;
	framecnt_t per_channel;
	bool at_end = false;

	abl.mNumberBuffers = 1;
	abl.mBuffers[0].mNumberChannels = channels();

	per_channel = nframes / abl.mBuffers[0].mNumberChannels;

	while (nread < per_channel) {

		UInt32 new_cnt = per_channel - nread;

		abl.mBuffers[0].mDataByteSize = new_cnt * abl.mBuffers[0].mNumberChannels * sizeof(Sample);
		abl.mBuffers[0].mData = buffer + nread;

		try {
			af.Read (new_cnt, &abl);
		} catch (CAXException& cax) {
			error << string_compose("CAImportable: %1", cax.mOperation);
			return -1;
		}

		if (new_cnt == 0) {
			/* EOF */
			at_end = true;
			break;
		}

		nread += new_cnt;
	}

	if (!at_end && nread < per_channel) {
		return 0;
	} else {
		return nread * abl.mBuffers[0].mNumberChannels;
	}
}

uint
CAImportableSource::channels () const
{
	return af.GetFileDataFormat().NumberChannels();
}

framecnt_t
CAImportableSource::length () const
{
	return af.GetNumberFrames();
}

framecnt_t
CAImportableSource::samplerate () const
{
	CAStreamBasicDescription client_asbd;

	try {
		client_asbd = af.GetClientDataFormat ();
	} catch (CAXException& cax) {
		error << string_compose ("CAImportable: %1", cax.mOperation) << endmsg;
		return 0.0;
	}

	return client_asbd.mSampleRate;
}

void
CAImportableSource::seek (framepos_t pos)
{
	try {
		af.Seek (pos);
	} catch (CAXException& cax) {
		error << string_compose ("CAImportable: %1 to %2", cax.mOperation, pos) << endmsg;
	}
}



