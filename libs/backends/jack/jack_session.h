/*
  Copyright (C) 2013 Paul Davis

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

#ifndef __ardour_jack_audiobackend_jack_session_h__
#define __ardour_jack_audiobackend_jack_session_h__

#include <jack/session.h>
#include <jack/transport.h>

#include "ardour/types.h"
#include "ardour/session_handle.h"

namespace ARDOUR {
	class Session;

class JACKSession : public ARDOUR::SessionHandlePtr
{
  public:
    JACKSession (ARDOUR::Session* s);
    ~JACKSession ();
    
    void session_event (jack_session_event_t* event);
    void timebase_callback (jack_transport_state_t /*state*/,
			    ARDOUR::pframes_t /*nframes*/,
			    jack_position_t* pos,
			    int /*new_position*/);
};

} /* namespace */

#endif /* __ardour_jack_audiobackend_jack_session_h__ */
