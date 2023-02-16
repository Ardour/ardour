/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_analyser_h__
#define __ardour_analyser_h__

#include <memory>

#include "ardour/libardour_visibility.h"
#include "pbd/pthread_utils.h"

namespace ARDOUR
{
class AudioFileSource;
class Source;

class LIBARDOUR_API Analyser
{
public:
	Analyser ();

	static void init ();
	static void terminate ();
	static void queue_source_for_analysis (std::shared_ptr<Source>, bool force);
	static void work ();
	static void flush ();

private:
	static Glib::Threads::Mutex               analysis_active_lock;
	static Glib::Threads::Mutex               analysis_queue_lock;
	static Glib::Threads::Cond                SourcesToAnalyse;
	static std::list<std::weak_ptr<Source>> analysis_queue;
	static bool                               analysis_thread_run;
	static PBD::Thread*                       analysis_thread;

	static void analyse_audio_file_source (std::shared_ptr<AudioFileSource>);
};

} // namespace ARDOUR

#endif /* __ardour_analyser_h__ */
