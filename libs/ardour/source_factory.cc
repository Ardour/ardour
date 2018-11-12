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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "pbd/error.h"
#include "pbd/convert.h"
#include "pbd/pthread_utils.h"
#include "pbd/stacktrace.h"

#include "ardour/audioplaylist.h"
#include "ardour/audio_playlist_source.h"
#include "ardour/boost_debug.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_playlist_source.h"
#include "ardour/source.h"
#include "ardour/source_factory.h"
#include "ardour/sndfilesource.h"
#include "ardour/silentfilesource.h"
#include "ardour/smf_source.h"
#include "ardour/session.h"

#ifdef  HAVE_COREAUDIO
#include "ardour/coreaudiosource.h"
#endif


#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

PBD::Signal1<void,boost::shared_ptr<Source> > SourceFactory::SourceCreated;
Glib::Threads::Cond SourceFactory::PeaksToBuild;
Glib::Threads::Mutex SourceFactory::peak_building_lock;
std::list<boost::weak_ptr<AudioSource> > SourceFactory::files_with_peaks;

static int active_threads = 0;

static void
peak_thread_work ()
{
	SessionEvent::create_per_thread_pool (X_("PeakFile Builder "), 64);

	while (true) {

		SourceFactory::peak_building_lock.lock ();

	  wait:
		if (SourceFactory::files_with_peaks.empty()) {
			SourceFactory::PeaksToBuild.wait (SourceFactory::peak_building_lock);
		}

		if (SourceFactory::files_with_peaks.empty()) {
			goto wait;
		}

		boost::shared_ptr<AudioSource> as (SourceFactory::files_with_peaks.front().lock());
		SourceFactory::files_with_peaks.pop_front ();
		++active_threads;
		SourceFactory::peak_building_lock.unlock ();

		if (!as) {
			continue;
		}

		as->setup_peakfile ();
		SourceFactory::peak_building_lock.lock ();
		--active_threads;
		SourceFactory::peak_building_lock.unlock ();
	}
}

int
SourceFactory::peak_work_queue_length ()
{
	// ideally we'd loop over the queue and check for duplicates
	// and existing valid peak-files..
	return SourceFactory::files_with_peaks.size () + active_threads;
}

void
SourceFactory::init ()
{
	for (int n = 0; n < 2; ++n) {
		Glib::Threads::Thread::create (sigc::ptr_fun (::peak_thread_work));
	}
}

int
SourceFactory::setup_peakfile (boost::shared_ptr<Source> s, bool async)
{
	boost::shared_ptr<AudioSource> as (boost::dynamic_pointer_cast<AudioSource> (s));

	if (as) {

		// immediately set 'peakfile-path' for empty and NoPeakFile sources
		if (async && !as->empty() && !(as->flags() & Source::NoPeakFile)) {

			Glib::Threads::Mutex::Lock lm (peak_building_lock);
			files_with_peaks.push_back (boost::weak_ptr<AudioSource> (as));
			PeaksToBuild.broadcast ();

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
SourceFactory::createSilent (Session& s, const XMLNode& node, samplecnt_t nframes, float sr)
{
	Source* src = new SilentFileSource (s, node, nframes, sr);
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
	// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif
	boost::shared_ptr<Source> ret (src);
	// no analysis data - the file is non-existent
	SourceCreated (ret);
	return ret;
}

boost::shared_ptr<Source>
SourceFactory::create (Session& s, const XMLNode& node, bool defer_peaks)
{
	DataType type = DataType::AUDIO;
	XMLProperty const * prop = node.property("type");

	if (prop) {
		type = DataType (prop->value());
	}

	if (type == DataType::AUDIO) {

		/* it could be nested */

		if (node.property ("playlist") != 0) {

			try {
				boost::shared_ptr<AudioPlaylistSource> ap (new AudioPlaylistSource (s, node));

				if (setup_peakfile (ap, true)) {
					return boost::shared_ptr<Source>();
				}

				ap->check_for_analysis_data_on_disk ();

				SourceCreated (ap);
				return ap;

			} catch (failed_constructor&) {
				/* oh well, so much for that then ... */
			}

		} else {


			try {
				Source* src = new SndFileSource (s, node);
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
				// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif
				boost::shared_ptr<Source> ret (src);
				if (setup_peakfile (ret, defer_peaks)) {
					return boost::shared_ptr<Source>();
				}
				ret->check_for_analysis_data_on_disk ();
				SourceCreated (ret);
				return ret;
			}

			catch (failed_constructor& err) {

#ifdef HAVE_COREAUDIO

				/* this is allowed to throw */

				Source *src = new CoreAudioSource (s, node);
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
				// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif
				boost::shared_ptr<Source> ret (src);

				if (setup_peakfile (ret, defer_peaks)) {
					return boost::shared_ptr<Source>();
				}

				ret->check_for_analysis_data_on_disk ();
				SourceCreated (ret);
				return ret;
#else
				throw; // rethrow
#endif
			}
		}
	} else if (type == DataType::MIDI) {
		try {
			boost::shared_ptr<SMFSource> src (new SMFSource (s, node));
			Source::Lock lock(src->mutex());
			src->load_model (lock, true);
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
			// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif
			src->check_for_analysis_data_on_disk ();
			SourceCreated (src);
			return src;
		} catch (...) {
		}
	}

	return boost::shared_ptr<Source>();
}

boost::shared_ptr<Source>
SourceFactory::createExternal (DataType type, Session& s, const string& path,
			       int chn, Source::Flag flags, bool announce, bool defer_peaks)
{
	if (type == DataType::AUDIO) {

		if (!(flags & Destructive)) {

			try {

				Source* src = new SndFileSource (s, path, chn, flags);
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
				// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif
				boost::shared_ptr<Source> ret (src);

				if (setup_peakfile (ret, defer_peaks)) {
					return boost::shared_ptr<Source>();
				}

				ret->check_for_analysis_data_on_disk ();
				if (announce) {
					SourceCreated (ret);
				}
				return ret;
			}

			catch (failed_constructor& err) {
#ifdef HAVE_COREAUDIO

				Source* src = new CoreAudioSource (s, path, chn, flags);
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
				// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif
				boost::shared_ptr<Source> ret (src);
				if (setup_peakfile (ret, defer_peaks)) {
					return boost::shared_ptr<Source>();
				}
				ret->check_for_analysis_data_on_disk ();
				if (announce) {
					SourceCreated (ret);
				}
				return ret;

#else
				throw; // rethrow
#endif
			}

		} else {
			// eh?
		}

	} else if (type == DataType::MIDI) {

		try {
			boost::shared_ptr<SMFSource> src (new SMFSource (s, path));
			Source::Lock lock(src->mutex());
			src->load_model (lock, true);
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
			// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif

			if (announce) {
				SourceCreated (src);
			}

			return src;
		} catch (...) {
		}

	}

	return boost::shared_ptr<Source>();
}

boost::shared_ptr<Source>
SourceFactory::createWritable (DataType type, Session& s, const std::string& path,
			       bool destructive, samplecnt_t rate, bool announce, bool defer_peaks)
{
	/* this might throw failed_constructor(), which is OK */

	if (type == DataType::AUDIO) {
		Source* src = new SndFileSource (s, path, string(),
						 s.config.get_native_file_data_format(),
						 s.config.get_native_file_header_format(),
						 rate,
						 (destructive
						  ? Source::Flag (SndFileSource::default_writable_flags | Source::Destructive)
						  : SndFileSource::default_writable_flags));
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
		// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif
		boost::shared_ptr<Source> ret (src);

		if (setup_peakfile (ret, defer_peaks)) {
			return boost::shared_ptr<Source>();
		}

		// no analysis data - this is a new file

		if (announce) {
			SourceCreated (ret);
		}
		return ret;

	} else if (type == DataType::MIDI) {
                // XXX writable flags should belong to MidiSource too
		try {
			boost::shared_ptr<SMFSource> src (new SMFSource (s, path, SndFileSource::default_writable_flags));

			assert (src->writable ());

			Source::Lock lock(src->mutex());
			src->load_model (lock, true);
#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
			// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif

			// no analysis data - this is a new file

			if (announce) {
				SourceCreated (src);
			}

			return src;

		} catch (...) {
		}
	}

	return boost::shared_ptr<Source> ();
}

boost::shared_ptr<Source>
SourceFactory::createForRecovery (DataType type, Session& s, const std::string& path, int chn)
{
	/* this might throw failed_constructor(), which is OK */

	if (type == DataType::AUDIO) {
		Source* src = new SndFileSource (s, path, chn);

#ifdef BOOST_SP_ENABLE_DEBUG_HOOKS
		// boost_debug_shared_ptr_mark_interesting (src, "Source");
#endif
		boost::shared_ptr<Source> ret (src);

		if (setup_peakfile (ret, false)) {
			return boost::shared_ptr<Source>();
		}

		// no analysis data - this is still basically a new file (we
		// crashed while recording.

		// always announce these files

		SourceCreated (ret);

		return ret;

	} else if (type == DataType::MIDI) {
		error << _("Recovery attempted on a MIDI file - not implemented") << endmsg;
	}

	return boost::shared_ptr<Source> ();
}

boost::shared_ptr<Source>
SourceFactory::createFromPlaylist (DataType type, Session& s, boost::shared_ptr<Playlist> p, const PBD::ID& orig, const std::string& name,
				   uint32_t chn, sampleoffset_t start, samplecnt_t len, bool copy, bool defer_peaks)
{
	if (type == DataType::AUDIO) {
		try {

			boost::shared_ptr<AudioPlaylist> ap = boost::dynamic_pointer_cast<AudioPlaylist>(p);

			if (ap) {

				if (copy) {
					ap.reset (new AudioPlaylist (ap, start, len, name, true));
					start = 0;
				}

				Source* src = new AudioPlaylistSource (s, orig, name, ap, chn, start, len, Source::Flag (0));
				boost::shared_ptr<Source> ret (src);

				if (setup_peakfile (ret, defer_peaks)) {
					return boost::shared_ptr<Source>();
				}

				ret->check_for_analysis_data_on_disk ();
				SourceCreated (ret);
				return ret;
			}
		}

		catch (failed_constructor& err) {
			/* relax - return at function scope */
		}

	} else if (type == DataType::MIDI) {

		try {

			boost::shared_ptr<MidiPlaylist> ap = boost::dynamic_pointer_cast<MidiPlaylist>(p);

			if (ap) {

				if (copy) {
					ap.reset (new MidiPlaylist (ap, start, len, name, true));
					start = 0;
				}

				Source* src = new MidiPlaylistSource (s, orig, name, ap, chn, start, len, Source::Flag (0));
				boost::shared_ptr<Source> ret (src);

				SourceCreated (ret);
				return ret;
			}
		}

		catch (failed_constructor& err) {
			/* relax - return at function scope */
		}

	}

	return boost::shared_ptr<Source>();
}
