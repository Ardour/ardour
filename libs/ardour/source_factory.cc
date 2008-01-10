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
#include <pbd/convert.h>
#include <pbd/pthread_utils.h>

#include <ardour/source_factory.h>
#include <ardour/sndfilesource.h>
#include <ardour/silentfilesource.h>
#include <ardour/configuration.h>
#include <ardour/smf_source.h>

#ifdef HAVE_COREAUDIO
#include <ardour/coreaudiosource.h>
#endif

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

sigc::signal<void,boost::shared_ptr<Source> > SourceFactory::SourceCreated;
Glib::Cond* SourceFactory::PeaksToBuild;
Glib::StaticMutex SourceFactory::peak_building_lock;
std::list<boost::weak_ptr<AudioSource> > SourceFactory::files_with_peaks;

static void 
peak_thread_work ()
{
	PBD::ThreadCreated (pthread_self(), string ("peakbuilder-") + to_string (pthread_self(), std::dec));

	while (true) {

		SourceFactory::peak_building_lock.lock ();
		
	  wait:
		if (SourceFactory::files_with_peaks.empty()) {
			SourceFactory::PeaksToBuild->wait (SourceFactory::peak_building_lock);
		}

		if (SourceFactory::files_with_peaks.empty()) {
			goto wait;
		}

		boost::shared_ptr<AudioSource> as (SourceFactory::files_with_peaks.front().lock());
		SourceFactory::files_with_peaks.pop_front ();
		SourceFactory::peak_building_lock.unlock ();
		
		if (!as) {
			continue;
		}
		as->setup_peakfile ();
	}
}

void
SourceFactory::init ()
{
	PeaksToBuild = new Glib::Cond();

	for (int n = 0; n < 2; ++n) {
		Glib::Thread::create (sigc::ptr_fun (::peak_thread_work), false);
	}
}

int
SourceFactory::setup_peakfile (boost::shared_ptr<Source> s, bool async)
{
	boost::shared_ptr<AudioSource> as (boost::dynamic_pointer_cast<AudioSource> (s));

	if (as) {

		if (async) {

			Glib::Mutex::Lock lm (peak_building_lock);
			files_with_peaks.push_back (boost::weak_ptr<AudioSource> (as));
			PeaksToBuild->broadcast ();

		} else {

			if (as->setup_peakfile ()) {
				error << string_compose("SourceFactory: could not set up peakfile for %1", as->name()) << endmsg;
				return -1;
			}
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

boost::shared_ptr<Source>
SourceFactory::create (Session& s, const XMLNode& node, bool defer_peaks)
{
       DataType type = DataType::AUDIO;
       const XMLProperty* prop = node.property("type");

       if (prop) {
               type = DataType(prop->value());
       }

       if (type == DataType::AUDIO) {

#ifdef HAVE_COREAUDIO

	       try {
		       boost::shared_ptr<Source> ret (new CoreAudioSource (s, node));
		       
		       if (setup_peakfile (ret, defer_peaks)) {
			       return boost::shared_ptr<Source>();
		       }
		       
		       SourceCreated (ret);
		       return ret;
	       } 
	       
	       
	       catch (failed_constructor& err) {	
		       
		       /* this is allowed to throw */
		       
		       boost::shared_ptr<Source> ret (new SndFileSource (s, node));
		       
		       if (setup_peakfile (ret, defer_peaks)) {
			       return boost::shared_ptr<Source>();
		       }
		       
		       SourceCreated (ret);
		       return ret;
	       }
#else
	       /* this is allowed to throw */

	       boost::shared_ptr<Source> ret (new SndFileSource (s, node));
	       
	       if (setup_peakfile (ret, defer_peaks)) {
		       return boost::shared_ptr<Source>();
	       }
	       
	       SourceCreated (ret);
	       return ret;
#endif
	
       } else if (type == DataType::MIDI) {
		   boost::shared_ptr<Source> ret (new SMFSource (s, node));
		       
		   SourceCreated (ret);
		   return ret;
       }

	return boost::shared_ptr<Source>();
}

boost::shared_ptr<Source>
SourceFactory::createReadable (DataType type, Session& s, string path, int chn, AudioFileSource::Flag flags, bool announce, bool defer_peaks)
{
	if (type == DataType::AUDIO) {
	
#ifdef HAVE_COREAUDIO
		try {
			boost::shared_ptr<Source> ret (new CoreAudioSource (s, path, chn, flags));

			if (setup_peakfile (ret, defer_peaks)) {
				return boost::shared_ptr<Source>();
			}

			if (announce) {
				SourceCreated (ret);
			}
			return ret;
		}
		
		catch (failed_constructor& err) {
			boost::shared_ptr<Source> ret (new SndFileSource (s, path, chn, flags));
			if (setup_peakfile (ret, defer_peaks)) {
				return boost::shared_ptr<Source>();
			}
			if (announce) {
				SourceCreated (ret);
			}
			return ret;
		}
#else
		boost::shared_ptr<Source> ret (new SndFileSource (s, path, chn, flags));

		if (setup_peakfile (ret, defer_peaks)) {
			return boost::shared_ptr<Source>();
		}

		if (announce) {
			SourceCreated (ret);
		}

		return ret;
#endif
		
	} else if (type == DataType::MIDI) {

		// FIXME: flags?
		boost::shared_ptr<Source> ret (new SMFSource (s, path, SMFSource::Flag(0)));

		if (announce) {
			SourceCreated (ret);
		}

		return ret;
	}

	return boost::shared_ptr<Source>();
}

boost::shared_ptr<Source>
SourceFactory::createWritable (DataType type, Session& s, std::string path, bool destructive, nframes_t rate, bool announce, bool defer_peaks)
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

		if (setup_peakfile (ret, defer_peaks)) {
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
