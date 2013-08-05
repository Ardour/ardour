/*
    Copyright (C) 2000,2007 Paul Davis

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

#ifndef __ardour_port_insert_h__
#define __ardour_port_insert_h__

#include <vector>
#include <string>
#include <exception>

#include "ardour/ardour.h"
#include "ardour/io_processor.h"
#include "ardour/types.h"

class XMLNode;
class MTDM;

namespace ARDOUR {

class Session;
class IO;
class Delivery;
class MuteMaster;
class Pannable;

/** Port inserts: send output to a Jack port, pick up input at a Jack port
 */
class PortInsert : public IOProcessor
{
  public:
	PortInsert (Session&, boost::shared_ptr<Pannable>, boost::shared_ptr<MuteMaster> mm);
	~PortInsert ();

	XMLNode& state(bool full);
	XMLNode& get_state(void);
	int set_state (const XMLNode&, int version);

	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);

	framecnt_t signal_latency () const;

	bool set_name (const std::string& name);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	void activate ();
	void deactivate ();

	uint32_t bit_slot() const { return _bitslot; }

	void start_latency_detection ();
	void stop_latency_detection ();

	MTDM* mtdm () const { return _mtdm; }
	void set_measured_latency (framecnt_t);
	framecnt_t latency () const;

	static std::string name_and_id_new_insert (Session&, uint32_t&);

  private:
	/* disallow copy construction */
	PortInsert (const PortInsert&);

	boost::shared_ptr<Delivery> _out;

	uint32_t   _bitslot;
	MTDM*      _mtdm;
	bool       _latency_detect;
	framecnt_t _latency_flush_frames;
	framecnt_t _measured_latency;
};

} // namespace ARDOUR

#endif /* __ardour_port_insert_h__ */
