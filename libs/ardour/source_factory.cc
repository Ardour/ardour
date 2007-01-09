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
#include <ardour/smf_source.h>
#include <ardour/destructive_filesource.h>
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

#ifdef HAVE_COREAUDIO
boost::shared_ptr<Source>
SourceFactory::create (Session& s, const XMLNode& node)
{
	DataType type = DataType::AUDIO;
	const XMLProperty* prop = node.property("type");
	if (prop) {
		type = DataType(prop->value());
	}

	if (type == DataType::AUDIO) {

		try {
			boost::shared_ptr<Source> ret (new CoreAudioSource (s, node));
			if (setup_peakfile (ret)) {
				return boost::shared_ptr<Source>();
			}
			SourceCreated (ret);
			return ret;
		} 

		catch (failed_constructor& err) {
			boost::shared_ptr<Source> ret (new SndFileSource (s, node));
			if (setup_peakfile (ret)) {
				return boost::shared_ptr<Source>();
			}
			SourceCreated (ret);
			return ret;
		}

	} else if (type == DataType::MIDI) {

		boost::shared_ptr<Source> ret (new SMFSource (node));
		SourceCreated (ret);
		return ret;

	}

	return boost::shared_ptr<Source>();
}

#else

boost::shared_ptr<Source>
SourceFactory::create (Session& s, const XMLNode& node)
{
	DataType type = DataType::AUDIO;
	const XMLProperty* prop = node.property("type");
	if (prop) {
		type = DataType(prop->value());
	}

	if (type == DataType::AUDIO) {
		
		boost::shared_ptr<Source> ret (new SndFileSource (s, node));

		if (setup_peakfile (ret)) {
			return boost::shared_ptr<Source>();
		}
		
		SourceCreated (ret);
		return ret;

	} else if (type == DataType::MIDI) {

		boost::shared_ptr<Source> ret (new SMFSource (s, node));
		
		SourceCreated (ret);
		return ret;

	}

	return boost::shared_ptr<Source> ();
}

#endif // HAVE_COREAUDIO

#ifdef HAVE_COREAUDIO
boost::shared_ptr<Source>
SourceFactory::createReadable (DataType type, Session& s, string idstr, AudioFileSource::Flag flags, bool announce)
{
	if (type == DataType::AUDIO) {

		if (!(flags & Destructive)) {

			try {
				boost::shared_ptr<Source> ret (new CoreAudioSource (s, idstr, flags));
				if (setup_peakfile (ret)) {
					return boost::shared_ptr<Source>();
				}
				if (announce) {
					SourceCreated (ret);
				}
				return ret;
			}

			catch (failed_constructor& err) {
				boost::shared_ptr<Source> ret (new SndFileSource (s, idstr, flags));
				if (setup_peakfile (ret)) {
					return boost::shared_ptr<Source>();
				}
				if (announce) {
					SourceCreated (ret);
				}
				return ret;
			}

		} else {

			boost::shared_ptr<Source> ret (new SndFileSource (s, idstr, flags));
			if (setup_peakfile (ret)) {
				return boost::shared_ptr<Source>();
			}
			if (announce) {
				SourceCreated (ret);
			}
			return ret;
		}

	} else if (type == DataType::MIDI) {

		boost::shared_ptr<Source> ret (new SMFSource (s, node));
		if (announce) {
			SourceCreated (ret);
		}
		return ret;

	}

	return boost::shared_ptr<Source>();
}

#else

boost::shared_ptr<Source>
SourceFactory::createReadable (DataType type, Session& s, string idstr, AudioFileSource::Flag flags, bool announce)
{
	if (type == DataType::AUDIO) {

		boost::shared_ptr<Source> ret (new SndFileSource (s, idstr, flags));

		if (setup_peakfile (ret)) {
			return boost::shared_ptr<Source>();
		}

		if (announce) {
			SourceCreated (ret);
		}

		return ret;

	} else if (type == DataType::MIDI) {

		// FIXME: flags?
		boost::shared_ptr<Source> ret (new SMFSource (s, idstr, SMFSource::Flag(0)));

		if (announce) {
			SourceCreated (ret);
		}

		return ret;
	}

	return boost::shared_ptr<Source>();
}

#endif // HAVE_COREAUDIO

boost::shared_ptr<Source>
SourceFactory::createWritable (DataType type, Session& s, std::string path, bool destructive, nframes_t rate, bool announce)
{
	/* this might throw failed_constructor(), which is OK */

	if (type == DataType::AUDIO) {
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

	} else if (type == DataType::MIDI) {

		boost::shared_ptr<Source> ret (new SMFSource (s, path));
		
		if (announce) {
			SourceCreated (ret);
		}
		return ret;

	}

	return boost::shared_ptr<Source> ();
}
