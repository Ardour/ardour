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

#include <glibmm/threads.h>
#include <boost/shared_ptr.hpp>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class AudioFileSource;
class Source;
class TransientDetector;

class LIBARDOUR_API Analyser {

  public:
	Analyser();
	~Analyser ();

	static void init ();
	static void queue_source_for_analysis (boost::shared_ptr<Source>, bool force);
	static void work ();
	static void flush ();

  private:
	static Analyser* the_analyser;
	static Glib::Threads::Mutex analysis_active_lock;
	static Glib::Threads::Mutex analysis_queue_lock;
	static Glib::Threads::Cond  SourcesToAnalyse;
	static std::list<boost::weak_ptr<Source> > analysis_queue;

	static void analyse_audio_file_source (boost::shared_ptr<AudioFileSource>);
};


}

#endif /* __ardour_analyser_h__ */
