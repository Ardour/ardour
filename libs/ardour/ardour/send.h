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

#include <sigc++/signal.h>
#include <string>

#include "pbd/stateful.h" 

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/delivery.h"

namespace ARDOUR {

class PeakMeter;
class Amp;

class Send : public Delivery
{
  public:	
	Send (Session&, boost::shared_ptr<MuteMaster>, Delivery::Role r = Delivery::Send);
	Send (Session&, boost::shared_ptr<MuteMaster>, const XMLNode&, Delivery::Role r = Delivery::Send);
	virtual ~Send ();

	uint32_t bit_slot() const { return _bitslot; }

	bool visible() const;

	boost::shared_ptr<Amp> amp() const { return _amp; }
	boost::shared_ptr<PeakMeter> meter() const { return _meter; }

	bool metering() const { return _metering; }
	void set_metering (bool yn) { _metering = yn; }
	
	XMLNode& state(bool full);
	XMLNode& get_state(void);
	int set_state(const XMLNode& node);

	uint32_t pans_required() const { return _configured_input.n_audio(); }

	void run (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;

	bool set_name (const std::string& str);

	static uint32_t how_many_sends();
	static void make_unique (XMLNode &, Session &);

  protected:
	bool _metering;
	boost::shared_ptr<Amp> _amp;
	boost::shared_ptr<PeakMeter> _meter;

  private:
	/* disallow copy construction */
	Send (const Send&);
	
	uint32_t  _bitslot;
};

} // namespace ARDOUR

#endif /* __ardour_send_h__ */
