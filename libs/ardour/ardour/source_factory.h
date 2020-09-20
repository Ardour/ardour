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

#include <string>
#include <stdint.h>
#include <boost/shared_ptr.hpp>

#include <glibmm/threads.h>

#include "ardour/source.h"

class XMLNode;

namespace ARDOUR {

class Session;
class AudioSource;
class Playlist;

class LIBARDOUR_API SourceFactory {
  public:
	static void init ();

	static PBD::Signal1<void,boost::shared_ptr<Source> > SourceCreated;

	static boost::shared_ptr<Source> create (Session&, const XMLNode& node, bool async = false);
	static boost::shared_ptr<Source> createSilent (Session&, const XMLNode& node,
	                                               samplecnt_t nframes, float sample_rate);

	static boost::shared_ptr<Source> createExternal
		(DataType type, Session&,
		 const std::string& path,
		 int chn, Source::Flag flags, bool announce = true, bool async = false);

	static boost::shared_ptr<Source> createWritable
		(DataType type, Session&,
		 const std::string& path,
		 samplecnt_t rate, bool announce = true, bool async = false);


	static boost::shared_ptr<Source> createForRecovery
		(DataType type, Session&, const std::string& path, int chn);

	static boost::shared_ptr<Source> createFromPlaylist
		(DataType type, Session& s, boost::shared_ptr<Playlist> p, const PBD::ID& orig, const std::string& name,
		 uint32_t chn, timepos_t start, timepos_t const & len, bool copy, bool defer_peaks);

        static Glib::Threads::Cond                       PeaksToBuild;
        static Glib::Threads::Mutex                      peak_building_lock;
	static std::list< boost::weak_ptr<AudioSource> > files_with_peaks;

	static int peak_work_queue_length ();
	static int setup_peakfile (boost::shared_ptr<Source>, bool async);
};

}

#endif /* __ardour_source_factory_h__ */
