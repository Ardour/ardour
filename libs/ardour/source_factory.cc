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

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

sigc::signal<void,boost::shared_ptr<Source> > SourceFactory::SourceCreated;

#ifdef HAVE_COREAUDIO


boost::shared_ptr<Source>
SourceFactory::create (const XMLNode& node)
{
	if (node.property (X_("destructive")) != 0) {
		
		boost::shared_ptr<Source> ret (new DestructiveFileSource (node));
		SourceCreated (ret);
		return ret;
		
	} else {
		
		try {
			boost::shared_ptr<Source> ret (new CoreAudioSource (node));
			SourceCreated (ret);
			return ret;
		} 
		
		
		catch (failed_constructor& err) {
			boost::shared_ptr<Source> ret (new SndFileSource (node));
			SourceCreated (ret);
			return ret;
		}
	}
	
	return boost::shared_ptr<Source>();
}

#else

boost::shared_ptr<Source>
SourceFactory::create (const XMLNode& node)
{
	if (node.property (X_("destructive")) != 0) {
		
		boost::shared_ptr<Source> ret (new DestructiveFileSource (node));
		SourceCreated (ret);
		return ret;
		
	} else {
		
		boost::shared_ptr<Source> ret (new SndFileSource (node));
		SourceCreated (ret);
		return ret;
	}
}

#endif // HAVE_COREAUDIO

#ifdef HAVE_COREAUDIO
boost::shared_ptr<Source>
SourceFactory::createReadable (string idstr, AudioFileSource::Flag flags, bool announce)
{
	if (flags & Destructive) {
		boost::shared_ptr<Source> ret (new DestructiveFileSource (idstr, flags));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}

	try {
		boost::shared_ptr<Source> ret (new CoreAudioSource (idstr, flags));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}

	catch (failed_constructor& err) {
		boost::shared_ptr<Source> ret (new SndFileSource (idstr, flags));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}

	return boost::shared_ptr<Source>();
}

#else

boost::shared_ptr<Source>
SourceFactory::createReadable (string idstr, AudioFileSource::Flag flags, bool announce)
{
	boost::shared_ptr<Source> ret (new SndFileSource (idstr, flags));
	if (announce) {
		SourceCreated (ret);
	}
	return ret;
}

#endif // HAVE_COREAUDIO

boost::shared_ptr<Source>
SourceFactory::createWritable (std::string path, bool destructive, jack_nframes_t rate, bool announce)
{
	/* this might throw failed_constructor(), which is OK */
	
	if (destructive) {
		boost::shared_ptr<Source> ret (new DestructiveFileSource (path,
									  Config->get_native_file_data_format(),
									  Config->get_native_file_header_format(),
									  rate));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
		
	} else {
		boost::shared_ptr<Source> ret (new SndFileSource (path, 
								  Config->get_native_file_data_format(),
								  Config->get_native_file_header_format(),
								  rate));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;
	}
}
