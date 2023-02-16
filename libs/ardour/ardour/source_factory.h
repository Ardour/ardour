/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_source_factory_h__
#define __ardour_source_factory_h__

#include <memory>
#include <stdint.h>
#include <string>

#include "pbd/pthread_utils.h"
#include "ardour/source.h"

class XMLNode;

namespace ARDOUR
{
class Session;
class AudioSource;
class Playlist;

class LIBARDOUR_API SourceFactory
{
public:
	static void init ();
	static void terminate ();

	static PBD::Signal1<void, std::shared_ptr<Source>> SourceCreated;

	static std::shared_ptr<Source> create (Session&, const XMLNode& node, bool async = false);
	static std::shared_ptr<Source> createSilent (Session&, const XMLNode& node, samplecnt_t, float sample_rate);
	static std::shared_ptr<Source> createExternal (DataType, Session&, const std::string& path, int chn, Source::Flag, bool announce = true, bool async = false);
	static std::shared_ptr<Source> createWritable (DataType, Session&, const std::string& path, samplecnt_t rate, bool announce = true, bool async = false);
	static std::shared_ptr<Source> createForRecovery (DataType, Session&, const std::string& path, int chn);
	static std::shared_ptr<Source> createFromPlaylist (DataType, Session&, std::shared_ptr<Playlist> p, const PBD::ID& orig, const std::string& name, uint32_t chn, timepos_t start, timepos_t const& len, bool copy, bool defer_peaks);

	static Glib::Threads::Cond  PeaksToBuild;
	static Glib::Threads::Mutex peak_building_lock;

	static bool                      peak_thread_run;
	static std::vector<PBD::Thread*> peak_thread_pool;

	static std::list<std::weak_ptr<AudioSource>> files_with_peaks;

	static int peak_work_queue_length ();
	static int setup_peakfile (std::shared_ptr<Source>, bool async);
};

} // namespace ARDOUR

#endif /* __ardour_source_factory_h__ */
