/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_delivery_h__
#define __ardour_delivery_h__

#include <string>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/chan_count.h"
#include "ardour/io_processor.h"
#include "ardour/gain_control.h"

namespace ARDOUR {

class BufferSet;
class IO;
class MuteMaster;
class PannerShell;
class Panner;
class Pannable;

class LIBARDOUR_API Delivery : public IOProcessor
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
		Aux    = 0x10,
		/* foldback - internal send used only to deliver to a personal monitor bus */
		Foldback = 0x20
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

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);

	/* supplemental method used with MIDI */

	void flush_buffers (samplecnt_t nframes);
	void no_outs_cuz_we_no_monitor(bool);
	void non_realtime_transport_stop (samplepos_t now, bool flush);
	void realtime_locate (bool);

	BufferSet& output_buffers() { return *_output_buffers; }

	PBD::Signal0<void> MuteChange;

	int set_state (const XMLNode&, int version);

	/* Panning */

	static int  disable_panners (void);
	static void reset_panners ();

	boost::shared_ptr<PannerShell> panner_shell() const { return _panshell; }
	boost::shared_ptr<Panner> panner() const;

	void add_gain (boost::shared_ptr<GainControl> gc) {
		_gain_control = gc;
	}

	void unpan ();
	void reset_panner ();
	void defer_pan_reset ();
	void allow_pan_reset ();

	uint32_t pans_required() const { return _configured_input.n_audio(); }
	virtual uint32_t pan_outs() const;

protected:
	XMLNode& state ();

	Role        _role;
	BufferSet*  _output_buffers;
	gain_t      _current_gain;
	boost::shared_ptr<PannerShell> _panshell;

	gain_t target_gain ();

private:
	bool _no_outs_cuz_we_no_monitor;

	boost::shared_ptr<MuteMaster>  _mute_master;
	boost::shared_ptr<GainControl> _gain_control;

	static bool panners_legal;
	static PBD::Signal0<void> PannersLegal;

	void panners_became_legal ();
	PBD::ScopedConnection panner_legal_c;
	void output_changed (IOChange, void*);

	bool _no_panner_reset;
};


} // namespace ARDOUR

#endif // __ardour__h__

