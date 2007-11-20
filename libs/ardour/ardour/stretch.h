/*
    Copyright (C) 2007 Paul Davis 

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

#ifndef __ardour_stretch_h__
#define __ardour_stretch_h__

#include <ardour/audiofilter.h>

#ifndef USE_RUBBERBAND
#include <soundtouch/SoundTouch.h>
#endif

namespace ARDOUR {

class AudioRegion;

struct TimeStretchRequest : public InterThreadInfo {
    float                fraction;
    bool                 quick_seek; 
    bool                 antialias;  
};

class Stretch : public AudioFilter {
  public:
	Stretch (ARDOUR::Session&, TimeStretchRequest&);
	~Stretch ();

	int run (boost::shared_ptr<ARDOUR::AudioRegion>);

  private:
	TimeStretchRequest& tsr;

#ifndef USE_RUBBERBAND
	soundtouch::SoundTouch st;
#endif

};

} /* namespace */

#endif /* __ardour_stretch_h__ */
