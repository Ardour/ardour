/*
 * Copyright (C) 2007-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
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

#ifndef __ardour_stretch_h__
#define __ardour_stretch_h__

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "ardour/filter.h"
#include "ardour/timefx_request.h"


#include "ardour/rb_effect.h"

namespace ARDOUR {

class LIBARDOUR_API RBStretch : public RBEffect {
  public:
	RBStretch (ARDOUR::Session&, TimeFXRequest&);
	~RBStretch() {}
};

} /* namespace */

#ifdef HAVE_SOUNDTOUCH
#include <soundtouch/SoundTouch.h>

namespace ARDOUR {

class LIBARDOUR_API STStretch : public Filter {
  public:
	STStretch (ARDOUR::Session&, TimeFXRequest&);
	~STStretch ();

	int run (boost::shared_ptr<ARDOUR::Region>, Progress* progress = 0);

  private:
	TimeFXRequest& tsr;
};

} /* namespace */
#endif


#endif /* __ardour_stretch_h__ */
