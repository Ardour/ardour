/*
    Copyright (C) 2012 Paul Davis

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

#include "ardour/ardour.h"
#include "ardour/session.h"
#include "ardour/sg_rack.h"
#include "ardour/debug.h"

using namespace ARDOUR;

SoundGridRack::SoundGridRack (Session& s, Route& r, const std::string& name)
        : SessionObject (s, name)
        , _route (r)
{
}

SoundGridRack::~SoundGridRack ()
{
}

void 
SoundGridRack::add_plugin (boost::shared_ptr<SoundGridPlugin>)
{
}

void 
SoundGridRack::remove_plugin (boost::shared_ptr<SoundGridPlugin>)
{
}

void
SoundGridRack::set_gain (gain_t)
{
}

void
SoundGridRack::set_input_gain (gain_t)
{
}
