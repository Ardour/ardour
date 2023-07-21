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

#include "pbd/signals.h"
#include "temporal/types.h"

namespace Temporal {

class TimeDomainProvider {
  public:
	explicit TimeDomainProvider () : have_domain (false), parent (nullptr) {}
	explicit TimeDomainProvider (TimeDomain td) : have_domain (true), domain (td), parent (nullptr) {}
	TimeDomainProvider (TimeDomain td, TimeDomainProvider const & p) : have_domain (true), domain (td), parent (&p) { listen(); }
	TimeDomainProvider (TimeDomainProvider const & other) : have_domain (other.have_domain), domain (other.domain), parent (other.parent) { listen(); }
	virtual ~TimeDomainProvider() {}

	TimeDomainProvider& operator= (TimeDomainProvider const & other) {
		if (this != &other) {
			parent_connection.disconnect ();
			have_domain = other.have_domain;
			domain = other.domain;
			parent = other.parent;
			listen ();
		}
		return *this;
	}

	TimeDomain time_domain() const { if (have_domain) return domain; if (parent) return parent->time_domain(); return AudioTime; }

	void clear_time_domain () { have_domain = false; TimeDomainChanged(); /* EMIT SIGNAL */ }
	void set_time_domain (TimeDomain td) { have_domain = true; domain = td; TimeDomainChanged(); /* EMIT SIGNAL */}

	bool has_parent() const { return (bool) parent; }
	void clear_time_domain_parent () { parent = nullptr; TimeDomainChanged (); /* EMIT SIGNAL */ }
	void set_time_domain_parent (TimeDomainProvider const & p) {
		parent_connection.disconnect ();
		TimeDomain old_domain = time_domain();
		parent = &p;
		TimeDomain new_domain = time_domain ();
		if (old_domain != new_domain) {
			TimeDomainChanged ();
		}
	}

	mutable PBD::Signal0<void> TimeDomainChanged;

	virtual void time_domain_changed() {
		/* Forward a time domain change to children */
		TimeDomainChanged();
	}

  protected:
	void listen () {
		if (parent) {
			parent->TimeDomainChanged.connect_same_thread (parent_connection, boost::bind (&TimeDomainProvider::time_domain_changed, this));
		}
	}

  private:
	bool have_domain;
	TimeDomain domain;
	TimeDomainProvider const * parent;
	PBD::ScopedConnection parent_connection;
};

}

#endif /* __temporal_domain_provider_h__ */
