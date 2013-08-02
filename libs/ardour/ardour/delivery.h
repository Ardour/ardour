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
class PannerShell;
class Panner;
class Pannable;

class Delivery : public IOProcessor
{
public:
	enum Role {
		/* main outputs - delivers out-of-place to port buffers, and cannot be removed */
		Main   = 0x1,
		/* send - delivers to port buffers, leaves input buffers untouched */
		Send   = 0x2,
		/* insert - delivers to port buffers and receives in-place from port buffers */
		Insert = 0x4,
		/* listen - internal send used only to deliver to control/monitor bus */
		Listen = 0x8,
		/* aux - internal send used to deliver to any bus, by user request */
		Aux    = 0x10
	};

	static bool role_requires_output_ports (Role r) { return r == Main || r == Send || r == Insert; }

	bool does_routing() const { return true; }

	/* Delivery to an existing output */

	Delivery (Session& s, boost::shared_ptr<IO> io, boost::shared_ptr<Pannable>, boost::shared_ptr<MuteMaster> mm, const std::string& name, Role);

	/* Delivery to a new output owned by this object */

	Delivery (Session& s, boost::shared_ptr<Pannable>, boost::shared_ptr<MuteMaster> mm, const std::string& name, Role);
	~Delivery ();

	bool set_name (const std::string& name);
	std::string display_name() const;

	Role role() const { return _role; }
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	void run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);

	/* supplemental method used with MIDI */

	void flush_buffers (framecnt_t nframes);
	void no_outs_cuz_we_no_monitor(bool);
	void transport_stopped (framepos_t frame);
	void realtime_locate ();

	BufferSet& output_buffers() { return *_output_buffers; }

	PBD::Signal0<void> MuteChange;

	XMLNode& state (bool full);
	int set_state (const XMLNode&, int version);

	/* Panning */

	static int  disable_panners (void);
	static void reset_panners ();

	boost::shared_ptr<PannerShell> panner_shell() const { return _panshell; }
	boost::shared_ptr<Panner> panner() const;

	void unpan ();
	void reset_panner ();
	void defer_pan_reset ();
	void allow_pan_reset ();

	uint32_t pans_required() const { return _configured_input.n_audio(); }
	virtual uint32_t pan_outs() const;

  protected:
	Role        _role;
	BufferSet*  _output_buffers;
	gain_t      _current_gain;
	boost::shared_ptr<PannerShell> _panshell;

	gain_t target_gain ();

  private:
	bool        _no_outs_cuz_we_no_monitor;
	boost::shared_ptr<MuteMaster> _mute_master;
	
	static bool panners_legal;
	static PBD::Signal0<void> PannersLegal;

	void panners_became_legal ();
	PBD::ScopedConnection panner_legal_c;
	void output_changed (IOChange, void*);

	bool _no_panner_reset;
};


} // namespace ARDOUR

#endif // __ardour__h__

