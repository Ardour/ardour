/*
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
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

	virtual void run (samplepos_t start, samplepos_t end) = 0;
	virtual void locate (samplepos_t where) = 0;
};

} /* namespace */


#endif /* __libardour_scene_change_h__ */
