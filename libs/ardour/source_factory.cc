/*
    Copyright (C) 2000-2006 Paul Davis 

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

#include <pbd/error.h>

#include <ardour/source_factory.h>
#include <ardour/sndfilesource.h>
#include <ardour/silentfilesource.h>
#include <ardour/configuration.h>

#ifdef HAVE_COREAUDIO
#include <ardour/coreaudiosource.h>
#endif

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

sigc::signal<void,boost::shared_ptr<Source> > SourceFactory::SourceCreated;

int
SourceFactory::setup_peakfile (boost::shared_ptr<Source> s)
{
	boost::shared_ptr<AudioSource> as (boost::dynamic_pointer_cast<AudioSource> (s));
	if (as) {
		if (as->setup_peakfile ()) {
			error << string_compose("SourceFactory: could not set up peakfile for %1", as->name()) << endmsg;
			return -1;
		}
	}

	return 0;
}

boost::shared_ptr<Source>
SourceFactory::createSilent (Session& s, const XMLNode& node, nframes_t nframes, float sr)
{
	boost::shared_ptr<Source> ret (new SilentFileSource (s, node, nframes, sr));
	SourceCreated (ret);
	return ret;
}

#ifdef HAVE_COREAUDIO
boost::shared_ptr<Source>
SourceFactory::create (Session& s, const XMLNode& node)
{
	try {
		boost::shared_ptr<Source> ret (new CoreAudioSource (s, node));
		if (setup_peakfile (ret)) {
			return boost::shared_ptr<Source>();
		}
		SourceCreated (ret);
		return ret;
	} 


	catch (failed_constructor& err) {	

		/* this is allowed to throw */

		boost::shared_ptr<Source> ret (new SndFileSource (s, node));
		if (setup_peakfile (ret)) {
			return boost::shared_ptr<Source>();
		}
		SourceCreated (ret);
		return ret;
	}

	return boost::shared_ptr<Source>();
}

#else

boost::shared_ptr<Source>
SourceFactory::create (Session& s, const XMLNode& node)
{
	/* this is allowed to throw */

	boost::shared_ptr<Source> ret (new SndFileSource (s, node));
	
	if (setup_peakfile (ret)) {
		return boost::shared_ptr<Source>();
	}
	
	SourceCreated (ret);
	return ret;
}

#endif // HAVE_COREAUDIO

#ifdef HAVE_COREAUDIO
boost::shared_ptr<Source>
SourceFactory::createReadable (Session& s, string path, int chn, AudioFileSource::Flag flags, bool announce)
{
	if (!(flags & Destructive)) {

		try {
			boost::shared_ptr<Source> ret (new CoreAudioSource (s, path, chn, flags));
			if (setup_peakfile (ret)) {
				return boost::shared_ptr<Source>();
			}
			if (announce) {
				SourceCreated (ret);
			}
			return ret;
		}
		
		catch (failed_constructor& err) {

			/* this is allowed to throw */

			boost::shared_ptr<Source> ret (new SndFileSource (s, path, chn, flags));
			if (setup_peakfile (ret)) {
				return boost::shared_ptr<Source>();
			}
			if (announce) {
				SourceCreated (ret);
			}
			return ret;
		}

	} else {

		boost::shared_ptr<Source> ret (new SndFileSource (s, path, chn, flags));
		if (setup_peakfile (ret)) {
			return boost::shared_ptr<Source>();
		}
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}

	return boost::shared_ptr<Source>();
}

#else

boost::shared_ptr<Source>
SourceFactory::createReadable (Session& s, string path, int chn, AudioFileSource::Flag flags, bool announce)
{
	boost::shared_ptr<Source> ret (new SndFileSource (s, path, chn, flags));

	if (setup_peakfile (ret)) {
		return boost::shared_ptr<Source>();
	}

	if (announce) {
		SourceCreated (ret);
	}

	return ret;
}

#endif // HAVE_COREAUDIO

boost::shared_ptr<Source>
SourceFactory::createWritable (Session& s, std::string path, bool destructive, nframes_t rate, bool announce)
{
	/* this might throw failed_constructor(), which is OK */

	boost::shared_ptr<Source> ret (new SndFileSource 
				       (s, path, 
					Config->get_native_file_data_format(),
					Config->get_native_file_header_format(),
					rate,
					(destructive ? AudioFileSource::Flag (SndFileSource::default_writable_flags | AudioFileSource::Destructive) :
					 SndFileSource::default_writable_flags)));	

	if (setup_peakfile (ret)) {
		return boost::shared_ptr<Source>();
	}
	if (announce) {
		SourceCreated (ret);
	}
	return ret;
}
