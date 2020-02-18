/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_rbeffect_h__
#define __ardour_rbeffect_h__

#include "ardour/filter.h"
#include "ardour/timefx_request.h"

namespace ARDOUR {

class AudioRegion;

class LIBARDOUR_API RBEffect : public Filter {
  public:
	RBEffect (ARDOUR::Session&, TimeFXRequest&);
	~RBEffect ();

	int run (boost::shared_ptr<ARDOUR::Region>, Progress* progress = 0);

  private:
	TimeFXRequest& tsr;
};

} /* namespace */

#endif /* __ardour_rbeffect_h__ */
