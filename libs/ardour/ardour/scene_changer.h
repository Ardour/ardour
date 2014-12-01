/*
    Copyright (C) 2014 Paul Davis

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

#ifndef __libardour_scene_changer_h__
#define __libardour_scene_changer_h__

#include <map>

#include "pbd/signals.h"

#include "ardour/location.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

namespace MIDI
{
class Parser;
class Port;
}

namespace ARDOUR 
{

class Session;
class AsyncMidiPort;

class SceneChanger : public SessionHandleRef
{
    public:
        SceneChanger (Session& s) : SessionHandleRef (s) {}
        virtual ~SceneChanger () {};
	
	virtual void run (framepos_t start, framepos_t end) = 0;
	virtual void locate (framepos_t where) = 0;
};

} /* namespace */
	

#endif /* __libardour_scene_change_h__ */
