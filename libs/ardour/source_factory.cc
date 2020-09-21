/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2009 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "pbd/error.h"
#include "pbd/convert.h"
#include "pbd/pthread_utils.h"

#include "ardour/audioplaylist.h"
#include "ardour/audio_playlist_source.h"
#include "ardour/boost_debug.h"
#include "ardour/ffmpegfilesource.h"
#include "ardour/midi_playlist.h"
#include "ardour/mp3filesource.h"
#include "ardour/source.h"
#include "ardour/source_factory.h"
#include "ardour/sndfilesource.h"
#include "ardour/silentfilesource.h"
#include "ardour/smf_source.h"
#include "ardour/session.h"

#ifdef HAVE_COREAUDIO
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
	pthread_set_name ("PeakFileBuilder");

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
	boost::shared_ptr<Source> ret (src);
	BOOST_MARK_SOURCE (ret);
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
					throw failed_constructor ();
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
				boost::shared_ptr<Source> ret (src);
				BOOST_MARK_SOURCE (ret);
				if (setup_peakfile (ret, defer_peaks)) {
					throw failed_constructor ();
				}
				ret->check_for_analysis_data_on_disk ();
				SourceCreated (ret);
				return ret;
			} catch (failed_constructor& err) { }

#ifdef HAVE_COREAUDIO
			try {
				Source* src = new CoreAudioSource (s, node);
				boost::shared_ptr<Source> ret (src);
				BOOST_MARK_SOURCE (ret);

				if (setup_peakfile (ret, defer_peaks)) {
					throw failed_constructor ();
				}

				ret->check_for_analysis_data_on_disk ();
				SourceCreated (ret);
				return ret;
			} catch (...) { }
#endif
			/* this is allowed to throw */
			throw failed_constructor ();
		}

	} else if (type == DataType::MIDI) {
		try {
			boost::shared_ptr<SMFSource> src (new SMFSource (s, node));
			Source::Lock lock(src->mutex());
			src->load_model (lock, true);
			BOOST_MARK_SOURCE (src);
			src->check_for_analysis_data_on_disk ();
			SourceCreated (src);
			return src;
		} catch (...) {
		}
	}

	throw failed_constructor ();
}

boost::shared_ptr<Source>
SourceFactory::createExternal (DataType type, Session& s, const string& path,
			       int chn, Source::Flag flags, bool announce, bool defer_peaks)
{
	if (type == DataType::AUDIO) {

		try {
			Source* src = new SndFileSource (s, path, chn, flags);
			boost::shared_ptr<Source> ret (src);
			BOOST_MARK_SOURCE (ret);
			if (setup_peakfile (ret, defer_peaks)) {
				throw failed_constructor ();
			}
			ret->check_for_analysis_data_on_disk ();
			if (announce) {
				SourceCreated (ret);
			}
			return ret;
		} catch (failed_constructor& err) { }

#ifdef HAVE_COREAUDIO
		try {
			Source* src = new CoreAudioSource (s, path, chn, flags);
			boost::shared_ptr<Source> ret (src);
			BOOST_MARK_SOURCE (ret);
			if (setup_peakfile (ret, defer_peaks)) {
				throw failed_constructor ();
			}
			ret->check_for_analysis_data_on_disk ();
			if (announce) {
				SourceCreated (ret);
			}
			return ret;
		} catch (...) { }
#endif

		/* only create mp3s for audition: no announce, no peaks */
		if (!announce && (!AudioFileSource::get_build_peakfiles () || defer_peaks)) {
			try {
				Source* src = new Mp3FileSource (s, path, chn, flags);
				boost::shared_ptr<Source> ret (src);
				BOOST_MARK_SOURCE (ret);
				return ret;

			} catch (failed_constructor& err) { }

			try {
				Source* src = new FFMPEGFileSource (s, path, chn, flags);
				boost::shared_ptr<Source> ret (src);
				BOOST_MARK_SOURCE (ret);
				return ret;

			} catch (failed_constructor& err) { }
		}

	} else if (type == DataType::MIDI) {

		try {
			boost::shared_ptr<SMFSource> src (new SMFSource (s, path));
			Source::Lock lock(src->mutex());
			src->load_model (lock, true);
			BOOST_MARK_SOURCE (src);

			if (announce) {
				SourceCreated (src);
			}

			return src;
		} catch (...) {
		}

	}

	throw failed_constructor ();
}

boost::shared_ptr<Source>
SourceFactory::createWritable (DataType type, Session& s, const std::string& path,
			       samplecnt_t rate, bool announce, bool defer_peaks)
{
	/* this might throw failed_constructor(), which is OK */

	if (type == DataType::AUDIO) {
		Source* src = new SndFileSource (s, path, string(),
						 s.config.get_native_file_data_format(),
						 s.config.get_native_file_header_format(),
						 rate,
		                                 SndFileSource::default_writable_flags);
		boost::shared_ptr<Source> ret (src);
		BOOST_MARK_SOURCE (ret);

		if (setup_peakfile (ret, defer_peaks)) {
			throw failed_constructor ();
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
			BOOST_MARK_SOURCE (src);

			// no analysis data - this is a new file

			if (announce) {
				SourceCreated (src);
			}

			return src;

		} catch (...) {
		}
	}

	throw failed_constructor ();
}

boost::shared_ptr<Source>
SourceFactory::createForRecovery (DataType type, Session& s, const std::string& path, int chn)
{
	/* this might throw failed_constructor(), which is OK */

	if (type == DataType::AUDIO) {
		Source* src = new SndFileSource (s, path, chn);

		boost::shared_ptr<Source> ret (src);
		BOOST_MARK_SOURCE (ret);

		if (setup_peakfile (ret, false)) {
			throw failed_constructor ();
		}

		// no analysis data - this is still basically a new file (we
		// crashed while recording.

		// always announce these files

		SourceCreated (ret);

		return ret;

	} else if (type == DataType::MIDI) {
		error << _("Recovery attempted on a MIDI file - not implemented") << endmsg;
	}

	throw failed_constructor ();
}

boost::shared_ptr<Source>
SourceFactory::createFromPlaylist (DataType type, Session& s, boost::shared_ptr<Playlist> p, const PBD::ID& orig, const std::string& name,
				   uint32_t chn, timepos_t start, timepos_t const & len, bool copy, bool defer_peaks)
{
	if (type == DataType::AUDIO) {
		try {

			boost::shared_ptr<AudioPlaylist> ap = boost::dynamic_pointer_cast<AudioPlaylist>(p);

			if (ap) {

				if (copy) {
					ap.reset (new AudioPlaylist (ap, start, len, name, true));
					start = timecnt_t::zero (Temporal::AudioTime);
				}

				Source* src = new AudioPlaylistSource (s, orig, name, ap, chn, start, len, Source::Flag (0));
				boost::shared_ptr<Source> ret (src);

				if (setup_peakfile (ret, defer_peaks)) {
					throw failed_constructor ();
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

		/* fail - not implemented, and probably too difficult to do */
	}

	throw failed_constructor ();
}
