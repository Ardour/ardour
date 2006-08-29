/*
    Copyright (C) 2004 Paul Davis 

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

#ifndef __ardour_audiofilter_h__
#define __ardour_audiofilter_h__

#include <vector>
#include <ardour/audioregion.h>

namespace ARDOUR {

class AudioRegion;
class Session;

class AudioFilter {

  public:
	AudioFilter (ARDOUR::Session& s)
		: session (s){}
	virtual ~AudioFilter() {}

	virtual int run (boost::shared_ptr<ARDOUR::AudioRegion>) = 0;
	std::vector<boost::shared_ptr<ARDOUR::AudioRegion> > results;

  protected:
	ARDOUR::Session& session;

	int make_new_sources (boost::shared_ptr<ARDOUR::AudioRegion>, ARDOUR::SourceList&);
	int finish (boost::shared_ptr<ARDOUR::AudioRegion>, ARDOUR::SourceList&);
};

} /* namespace */

#endif /* __ardour_audiofilter_h__ */
