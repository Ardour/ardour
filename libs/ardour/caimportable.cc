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
		error << string_compose ("CAImportable: %1", cax.mOperation) << endmsg;
		throw failed_constructor ();
	}

}

CAImportableSource::~CAImportableSource ()
{
}

nframes_t
CAImportableSource::read (Sample* buffer, nframes_t nframes) 
{
	nframes_t nread = 0;
	AudioBufferList abl;
	nframes_t per_channel;
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
		return nread * 	abl.mBuffers[0].mNumberChannels;
	}
}

uint
CAImportableSource::channels () const 
{
	return af.GetFileDataFormat().NumberChannels();
}

nframes_t
CAImportableSource::length () const 
{
	return af.GetNumberFrames();
}

nframes_t
CAImportableSource::samplerate() const
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
CAImportableSource::seek (nframes_t pos)
{
	try {
		af.Seek (pos);
	} catch (CAXException& cax) {
		error << string_compose ("CAImportable: %1 to %2", cax.mOperation, pos) << endmsg;
	}
}

	

