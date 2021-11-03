/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

#ifndef __ardour_internal_send_h__
#define __ardour_internal_send_h__

#include "ardour/ardour.h"
#include "ardour/send.h"

namespace ARDOUR {

class LIBARDOUR_API InternalSend : public Send
{
public:
	InternalSend (Session&, boost::shared_ptr<Pannable>, boost::shared_ptr<MuteMaster>, boost::shared_ptr<Route> send_from, boost::shared_ptr<Route> send_to, Delivery::Role role = Delivery::Aux, bool ignore_bitslot = false);
	virtual ~InternalSend ();

	std::string display_name() const;
	bool set_name (const std::string&);
	bool visible() const;

	int set_state(const XMLNode& node, int version);

	void cycle_start (pframes_t);
	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);
	bool feeds (boost::shared_ptr<Route> other) const;
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);
	int  set_block_size (pframes_t);

	boost::shared_ptr<Route> source_route() const { return _send_from; }
	boost::shared_ptr<Route> target_route() const { return _send_to; }
	const PBD::ID& target_id() const { return _send_to_id; }

	BufferSet const & get_buffers () const {
		return mixbufs;
	}

	bool allow_feedback () const { return _allow_feedback;}
	void set_allow_feedback (bool yn);

	void set_can_pan (bool yn);
	uint32_t pan_outs () const;

	static PBD::Signal1<void, pframes_t> CycleStart;

protected:
	XMLNode& state();

private:
	BufferSet mixbufs;
	boost::shared_ptr<Route> _send_from;
	boost::shared_ptr<Route> _send_to;
	bool _allow_feedback;
	PBD::ID _send_to_id;
	PBD::ScopedConnection connect_c;
	PBD::ScopedConnection source_connection;
	PBD::ScopedConnectionList target_connections;

	void send_from_going_away ();
	void send_to_going_away ();
	void send_to_property_changed (const PBD::PropertyChange&);
	int  after_connect ();
	void init_gain ();
	int  use_target (boost::shared_ptr<Route>, bool update_name = true);
	void target_io_changed ();
	void ensure_mixbufs ();

	void propagate_solo ();
};

} // namespace ARDOUR

#endif /* __ardour_send_h__ */
