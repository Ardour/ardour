/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_send_h__
#define __ardour_send_h__

#include <string>

#include "pbd/stateful.h"

#include "ardour/ardour.h"
#include "ardour/delivery.h"

namespace ARDOUR {

class PeakMeter;
class Amp;

class LIBARDOUR_API Send : public Delivery
{
  public:
	Send (Session&, boost::shared_ptr<Pannable> pannable, boost::shared_ptr<MuteMaster>, Delivery::Role r = Delivery::Send);
	virtual ~Send ();

	uint32_t bit_slot() const { return _bitslot; }

	bool display_to_user() const;

	boost::shared_ptr<Amp> amp() const { return _amp; }
	boost::shared_ptr<PeakMeter> meter() const { return _meter; }

	bool metering() const { return _metering; }
	void set_metering (bool yn) { _metering = yn; }

	XMLNode& state (bool full);
	XMLNode& get_state ();
	int set_state(const XMLNode&, int version);

	uint32_t pans_required() const { return _configured_input.n_audio(); }

	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	void activate ();
	void deactivate ();

	bool set_name (const std::string& str);

	std::string value_as_string (boost::shared_ptr<AutomationControl>) const;
	
	static uint32_t how_many_sends();
	static std::string name_and_id_new_send (Session&, Delivery::Role r, uint32_t&);

  protected:
	bool _metering;
	boost::shared_ptr<Amp> _amp;
	boost::shared_ptr<PeakMeter> _meter;

  private:
	/* disallow copy construction */
	Send (const Send&);

	int set_state_2X (XMLNode const &, int);

	uint32_t  _bitslot;
};

} // namespace ARDOUR

#endif /* __ardour_send_h__ */
