/*
    Copyright (C) 2000-2007 Paul Davis 

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

*/

#ifndef __ardour_source_factory_h__
#define __ardour_source_factory_h__

#include <string>
#include <stdint.h>
#include <sigc++/sigc++.h>
#include <boost/shared_ptr.hpp>

#include <ardour/source.h>
#include <ardour/audiofilesource.h>

class XMLNode;

namespace ARDOUR {

class Session;

class SourceFactory {
  public:
	static sigc::signal<void,boost::shared_ptr<Source> > SourceCreated;

	static boost::shared_ptr<Source> create (Session&, const XMLNode& node);
	static boost::shared_ptr<Source> createSilent (Session&, const XMLNode& node, nframes_t nframes, float sample_rate);

	// MIDI sources will have to be hacked in here somehow
	static boost::shared_ptr<Source> createReadable (Session&, std::string path, int chn, AudioFileSource::Flag flags, bool announce = true);
	static boost::shared_ptr<Source> createWritable (Session&, std::string name, bool destructive, nframes_t rate, bool announce = true);

  private:
	static int setup_peakfile (boost::shared_ptr<Source>);
};

}

#endif /* __ardour_source_factory_h__ */
