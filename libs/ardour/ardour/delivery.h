/*
    Copyright (C) 2006 Paul Davis 
    
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
    
    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_delivery_h__
#define __ardour_delivery_h__

#include <string>
#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/io_processor.h"

namespace ARDOUR {

class BufferSet;
class IO;
class MuteMaster;
class Panner;

class Delivery : public IOProcessor {
public:
	enum Role {
		Insert = 0x1,
		Send   = 0x2,
		Listen = 0x4,
		Main   = 0x8,
		Aux    = 0x10
	};

	/* Delivery to an existing output */

	Delivery (Session& s, boost::shared_ptr<IO> io, boost::shared_ptr<MuteMaster> mm, const std::string& name, Role);
	Delivery (Session& s, boost::shared_ptr<IO> io, boost::shared_ptr<MuteMaster> mm, const XMLNode&);

	/* Delivery to a new output owned by this object */

	Delivery (Session& s, boost::shared_ptr<MuteMaster> mm, const std::string& name, Role);
	Delivery (Session&, boost::shared_ptr<MuteMaster> mm, const XMLNode&);

	bool set_name (const std::string& name);
	std::string display_name() const;

	bool visible() const;
	Role role() const { return _role; }
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	bool configure_io (ChanCount in, ChanCount out);

	void run (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes);

	/* supplemental method use with MIDI */

	void flush (nframes_t nframes);

	void no_outs_cuz_we_no_monitor(bool);

	void set_solo_level (int32_t sl) { _solo_level = sl; }
	void set_solo_isolated (bool yn) { _solo_isolated = yn; }

	void cycle_start (nframes_t);
	void increment_output_offset (nframes_t);
	void transport_stopped (sframes_t frame);

	BufferSet& output_buffers() { return *_output_buffers; }

	sigc::signal<void> MuteChange;

	static sigc::signal<void,nframes_t> CycleStart;

	XMLNode& state (bool full);
	int set_state (const XMLNode&);

	/* Panning */

	static int  disable_panners (void);
	static int  reset_panners (void);

	boost::shared_ptr<Panner> panner() const { return _panner; }

	void reset_panner ();
	void defer_pan_reset ();
	void allow_pan_reset ();

	uint32_t pans_required() const { return _configured_input.n_audio(); }
	void start_pan_touch (uint32_t which);
	void end_pan_touch (uint32_t which);

  protected:
	Role        _role;
	BufferSet*  _output_buffers;
	gain_t      _current_gain;
	nframes_t   _output_offset;
	bool        _no_outs_cuz_we_no_monitor;
	uint32_t    _solo_level;
	bool        _solo_isolated;
	boost::shared_ptr<MuteMaster> _mute_master;
	bool         no_panner_reset;
	boost::shared_ptr<Panner> _panner;

	static bool panners_legal;
	static sigc::signal<int>            PannersLegal;

	int panners_became_legal ();
	sigc::connection panner_legal_c;
	void output_changed (IOChange, void*);

	gain_t target_gain ();
};


} // namespace ARDOUR

#endif // __ardour__h__

