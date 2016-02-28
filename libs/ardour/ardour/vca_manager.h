/*
  Copyright (C) 2016 Paul Davis

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

#ifndef __libardour_vca_manager_h__
#define __libardour_vca_manager_h__

#include <string>
#include <list>

#include <boost/shared_ptr.hpp>

#include <glibmm/threads.h>

#include "pbd/signals.h"

#include "ardour/session_handle.h"
#include "ardour/types.h"

namespace ARDOUR {

class VCA;

class VCAManager : public SessionHandleRef
{
     public:
	VCAManager (ARDOUR::Session&);
	~VCAManager ();

	boost::shared_ptr<VCA> create_vca (std::string const & name = std::string());
	void remove_vca (boost::shared_ptr<VCA>);

	typedef std::list<boost::shared_ptr<VCA> > VCAS;
	VCAS vcas() const;

	PBD::Signal1<void,VCAList&> VCAAdded;
	PBD::Signal1<void,VCAList&> VCARemoved;

     private:
	mutable Glib::Threads::Mutex lock;
	VCAS _vcas;

};

} // namespace

#endif /* __libardour_vca_manager_h__ */
