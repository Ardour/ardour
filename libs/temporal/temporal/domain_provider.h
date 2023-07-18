/*
    Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>

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

#ifndef __temporal_domain_provider_h__
#define __temporal_domain_provider_h__

#include "temporal/types.h"

namespace Temporal {

class TimeDomainProvider {
  public:
	TimeDomainProvider () : have_domain (false), parent (nullptr) {}
	TimeDomainProvider (TimeDomain td) : have_domain (true), domain (td), parent (nullptr) {}
	TimeDomainProvider (TimeDomain td, TimeDomainProvider const & p) : have_domain (true), domain (td), parent (&p) {}
	TimeDomainProvider (TimeDomainProvider const & p) : have_domain (false), parent (&p) {}
	virtual ~TimeDomainProvider() {}

	TimeDomain time_domain() const { if (have_domain) return domain; if (parent) return parent->time_domain(); return AudioTime; }
	void set_time_domain (TimeDomain td) { have_domain = true; domain = td; }
	void clear_time_domain () { have_domain = false; }

  private:
	bool have_domain;
	TimeDomain domain;
	TimeDomainProvider const * parent;
};

}

#endif /* __temporal_domain_provider_h__ */
