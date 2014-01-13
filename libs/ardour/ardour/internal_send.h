/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __ardour_internal_send_h__
#define __ardour_internal_send_h__

#include "ardour/ardour.h"
#include "ardour/send.h"

namespace ARDOUR {

class LIBARDOUR_API InternalSend : public Send
{
  public:
	InternalSend (Session&, boost::shared_ptr<Pannable>, boost::shared_ptr<MuteMaster>, boost::shared_ptr<Route> send_to, Delivery::Role role);
	virtual ~InternalSend ();

	std::string display_name() const;
	bool set_name (const std::string&);
	bool visible() const;

	XMLNode& state(bool full);
	XMLNode& get_state(void);
	int set_state(const XMLNode& node, int version);

	void cycle_start (pframes_t);
	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);
	bool feeds (boost::shared_ptr<Route> other) const;
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);
	int  set_block_size (pframes_t);

	boost::shared_ptr<Route> target_route() const { return _send_to; }
	const PBD::ID& target_id() const { return _send_to_id; }

	BufferSet const & get_buffers () const {
		return mixbufs;
	}

	void set_can_pan (bool yn);
	uint32_t pan_outs () const;

	static PBD::Signal1<void, pframes_t> CycleStart;

  private:
	BufferSet mixbufs;
	boost::shared_ptr<Route> _send_to;
	PBD::ID _send_to_id;
	PBD::ScopedConnection connect_c;
	PBD::ScopedConnectionList target_connections;

	void send_to_going_away ();
	void send_to_property_changed (const PBD::PropertyChange&);
	int  connect_when_legal ();
	void init_gain ();
	int  use_target (boost::shared_ptr<Route>);
	void target_io_changed ();
};

} // namespace ARDOUR

#endif /* __ardour_send_h__ */
