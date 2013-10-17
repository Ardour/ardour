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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "ardour/filter.h"
#include "ardour/timefx_request.h"

#ifdef USE_RUBBERBAND

#include "ardour/rb_effect.h"

namespace ARDOUR {

class LIBARDOUR_API RBStretch : public RBEffect {
  public:
	RBStretch (ARDOUR::Session&, TimeFXRequest&);
	~RBStretch() {}
};

} /* namespace */

#else

#include <soundtouch/SoundTouch.h>

namespace ARDOUR {

class LIBARDOUR_API STStretch : public Filter {
  public:
	STStretch (ARDOUR::Session&, TimeFXRequest&);
	~STStretch ();

	int run (boost::shared_ptr<ARDOUR::Region>);

  private:
	TimeFXRequest& tsr;

	soundtouch::SoundTouch st;
};

} /* namespace */

#endif

#endif /* __ardour_stretch_h__ */
