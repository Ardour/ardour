/*
 * Copyright (C) 2019 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _ardour_transport_api_h_
#define _ardour_transport_api_h_

#include "ardour/types.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR
{

class LIBARDOUR_API TransportAPI
{
   public:
	TransportAPI() {}
	virtual ~TransportAPI() {}

  private:
	friend struct TransportFSM;

	virtual void locate (samplepos_t, bool with_loop=false, bool force=false, bool with_mmc=true) = 0;
	virtual bool should_stop_before_locate () const = 0;
	virtual void stop_transport (bool abort = false, bool clear_state = false) = 0;
	virtual void start_transport (bool after_loop) = 0;
	virtual void butler_completed_transport_work () = 0;
	virtual void schedule_butler_for_transport_work () = 0;
	virtual bool should_roll_after_locate () const = 0;
	virtual bool user_roll_after_locate() const = 0;
	virtual void set_transport_speed (double speed) = 0;
	virtual samplepos_t position() const = 0;
   	virtual bool need_declick_before_locate () const = 0;
};

} /* end namespace ARDOUR */

#endif
