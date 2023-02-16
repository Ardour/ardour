/*
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#include "ardour/analyser.h"
#include "ardour/audiofilesource.h"
#include "ardour/rc_configuration.h"
#include "ardour/session_event.h"
#include "ardour/transient_detector.h"

#include "pbd/compose.h"
#include "pbd/error.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Glib::Threads::Mutex          Analyser::analysis_active_lock;
Glib::Threads::Mutex          Analyser::analysis_queue_lock;
Glib::Threads::Cond           Analyser::SourcesToAnalyse;
list<std::weak_ptr<Source>> Analyser::analysis_queue;
bool                          Analyser::analysis_thread_run = false;
PBD::Thread*                  Analyser::analysis_thread     = 0;

Analyser::Analyser ()
{
}

void
Analyser::init ()
{
	if (analysis_thread_run) {
		return;
	}
	analysis_thread_run = true;
	analysis_thread     = PBD::Thread::create (sigc::ptr_fun (&Analyser::work), "Analyzer");
}

void
Analyser::terminate ()
{
	if (!analysis_thread_run) {
		return;
	}
	analysis_thread_run = false;
	SourcesToAnalyse.broadcast ();
	analysis_thread->join ();
}

void
Analyser::queue_source_for_analysis (std::shared_ptr<Source> src, bool force)
{
	if (!src->can_be_analysed ()) {
		return;
	}

	if (!force && src->has_been_analysed ()) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (analysis_queue_lock);
	analysis_queue.push_back (std::weak_ptr<Source> (src));
	SourcesToAnalyse.broadcast ();
}

void
Analyser::work ()
{
	SessionEvent::create_per_thread_pool ("Analyser", 64);

	while (true) {
		analysis_queue_lock.lock ();

	wait:
		if (analysis_queue.empty () && analysis_thread_run) {
			SourcesToAnalyse.wait (analysis_queue_lock);
		}

		if (!analysis_thread_run) {
			analysis_queue_lock.unlock ();
			break;
		}

		if (analysis_queue.empty ()) {
			goto wait;
		}

		std::shared_ptr<Source> src (analysis_queue.front ().lock ());
		analysis_queue.pop_front ();
		analysis_queue_lock.unlock ();

		std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource> (src);

		if (afs && !afs->empty ()) {
			Glib::Threads::Mutex::Lock lm (analysis_active_lock);
			analyse_audio_file_source (afs);
		}
	}
}

void
Analyser::analyse_audio_file_source (std::shared_ptr<AudioFileSource> src)
{
	AnalysisFeatureList results;

	try {
		TransientDetector td (src->sample_rate ());
		td.set_sensitivity (3, Config->get_transient_sensitivity ()); // "General purpose"
		if (td.run (src->get_transients_path (), src.get (), 0, results) == 0) {
			src->set_been_analysed (true);
		} else {
			src->set_been_analysed (false);
		}
	} catch (...) {
		error << string_compose (_ ("Transient Analysis failed for %1."), _ ("Audio File Source")) << endmsg;
		;
		src->set_been_analysed (false);
		return;
	}
}

void
Analyser::flush ()
{
	Glib::Threads::Mutex::Lock lq (analysis_queue_lock);
	Glib::Threads::Mutex::Lock la (analysis_active_lock);
	analysis_queue.clear ();
}
