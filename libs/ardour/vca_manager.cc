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

#include "ardour/vca.h"
#include "ardour/vca_manager.h"

using namespace ARDOUR;
using namespace Glib::Threads;
using std::string;


VCAManager::VCAManager (Session& s)
	: SessionHandleRef (s)
{
}

VCAManager::~VCAManager ()
{
}

VCAManager::VCAS
VCAManager::vcas () const
{
	Mutex::Lock lm (lock);
	return _vcas;
}

boost::shared_ptr<VCA>
VCAManager::create_vca (std::string const & name)
{
	boost::shared_ptr<VCA> vca = boost::shared_ptr<VCA> (new VCA (_session, name));
	{
		Mutex::Lock lm (lock);
		_vcas.push_back (vca);
	}

	VCAAdded (vca); /* EMIT SIGNAL */
	return vca;

}


void
VCAManager::remove_vca (boost::shared_ptr<VCA> vca)
{
	{
		Mutex::Lock lm (lock);
		_vcas.remove (vca);
	}

	VCARemoved (vca); /* EMIT SIGNAL */
}

