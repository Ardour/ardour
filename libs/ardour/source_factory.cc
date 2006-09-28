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

#include <ardour/source_factory.h>
#include <ardour/sndfilesource.h>
#include <ardour/destructive_filesource.h>
#include <ardour/configuration.h>

#ifdef HAVE_COREAUDIO
#include <ardour/coreaudiosource.h>
#endif

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

sigc::signal<void,boost::shared_ptr<Source> > SourceFactory::SourceCreated;

#ifdef HAVE_COREAUDIO
boost::shared_ptr<Source>
SourceFactory::create (Session& s, const XMLNode& node)
{
	if (node.property (X_("destructive")) != 0) {
		
		boost::shared_ptr<Source> ret (new DestructiveFileSource (s, node));
		SourceCreated (ret);
		return ret;
		
	} else {
		
		try {
			boost::shared_ptr<Source> ret (new CoreAudioSource (s, node));
			SourceCreated (ret);
			return ret;
		} 
		
		
		catch (failed_constructor& err) {
			boost::shared_ptr<Source> ret (new SndFileSource (s, node));
			SourceCreated (ret);
			return ret;
		}
	}
	
	return boost::shared_ptr<Source>();
}

#else

boost::shared_ptr<Source>
SourceFactory::create (Session& s, const XMLNode& node)
{
	if (node.property (X_("destructive")) != 0) {
		
		boost::shared_ptr<Source> ret (new DestructiveFileSource (s, node));
		SourceCreated (ret);
		return ret;
		
	} else {
		
		boost::shared_ptr<Source> ret (new SndFileSource (s, node));
		SourceCreated (ret);
		return ret;
	}
}

#endif // HAVE_COREAUDIO

#ifdef HAVE_COREAUDIO
boost::shared_ptr<Source>
SourceFactory::createReadable (Session& s, string idstr, AudioFileSource::Flag flags, bool announce)
{
	if (flags & Destructive) {
		boost::shared_ptr<Source> ret (new DestructiveFileSource (s, idstr, flags));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}

	try {
		boost::shared_ptr<Source> ret (new CoreAudioSource (s, idstr, flags));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}

	catch (failed_constructor& err) {
		boost::shared_ptr<Source> ret (new SndFileSource (s, idstr, flags));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}

	return boost::shared_ptr<Source>();
}

#else

boost::shared_ptr<Source>
SourceFactory::createReadable (Session& s, string idstr, AudioFileSource::Flag flags, bool announce)
{
	boost::shared_ptr<Source> ret (new SndFileSource (s, idstr, flags));
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
	
	if (destructive) {
		boost::shared_ptr<Source> ret (new DestructiveFileSource (s, path,
									  Config->get_native_file_data_format(),
									  Config->get_native_file_header_format(),
									  rate));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
		
	} else {
		boost::shared_ptr<Source> ret (new SndFileSource (s, path, 
								  Config->get_native_file_data_format(),
								  Config->get_native_file_header_format(),
								  rate));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}
}
