/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_processor_h__
#define __ardour_processor_h__

#include <vector>
#include <string>
#include <exception>

#include "pbd/statefuldestructible.h"

#include "ardour/ardour.h"
#include "ardour/buffer_set.h"
#include "ardour/latent.h"
#include "ardour/session_object.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/automatable.h"

class XMLNode;
class ProcessorWindowProxy;
class PluginPinWindowProxy;

namespace ARDOUR {

class Location;
class Session;

/** A mixer strip element - plugin, send, meter, etc */
class LIBARDOUR_API Processor : public SessionObject, public Automatable, public Latent
{
  public:
	static const std::string state_node_name;

	Processor(Session&, const std::string& name, Temporal::TimeDomain);
	Processor (const Processor& other);

	virtual ~Processor();

	virtual std::string display_name() const { return SessionObject::name(); }

	virtual bool display_to_user() const { return _display_to_user; }
	virtual void set_display_to_user (bool);

	bool active () const { return _pending_active; } ///< ardour hard bypass
	virtual bool enabled () const { return _pending_active; } ///< processor enabled/bypass
	virtual bool bypassable () const { return true; } ///< enable is not automated or locked

	virtual bool does_routing() const { return false; }

	bool get_next_ab_is_active () const { return _next_ab_is_active; }
	void set_next_ab_is_active (bool yn) { _next_ab_is_active = yn; }

	virtual samplecnt_t signal_latency() const { return 0; }

	virtual void set_input_latency (samplecnt_t cnt) { _input_latency = cnt; }
	samplecnt_t input_latency () const               { return _input_latency; }

	virtual void set_output_latency (samplecnt_t cnt) { _output_latency = cnt; }
	samplecnt_t output_latency () const               { return _output_latency; }

	virtual void set_capture_offset (samplecnt_t cnt) { _capture_offset = cnt; }
	samplecnt_t capture_offset () const               { return _capture_offset; }

	virtual void set_playback_offset (samplecnt_t cnt) { _playback_offset = cnt; }
	samplecnt_t playback_offset () const               { return _playback_offset; }

	virtual int set_block_size (pframes_t /*nframes*/) { return 0; }
	virtual bool requires_fixed_sized_buffers() const { return false; }

	/** The main process function for processors
	 *
	 * @param bufs bufferset of data to process in-place
	 * @param start_sample absolute timeline position in audio-samples to commence processing (latency compensated)
	 * @param end_sample absolute timeline position in audio-samples, usually start_sample +/- \p nframes
	 * @param speed transport speed. usually -1, 0, +1
	 * @param nframes number of audio samples to process
	 * @param result_required true if, on return from this method, \p bufs is required to contain valid data;
	 *        if false, the method need not bother writing to @a bufs if it doesn't want to.
	 */
	virtual void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required) {}
	virtual void silence (samplecnt_t nframes, samplepos_t start_sample) { automation_run (start_sample, nframes); }

	virtual void activate ()   { _pending_active = true; ActiveChanged(); }
	virtual void deactivate () { _pending_active = false; ActiveChanged(); }
	virtual void flush() {}

	virtual void enable (bool yn) { if (yn) { activate (); } else { deactivate (); } }

	virtual bool configure_io (ChanCount in, ChanCount out);

	/* Derived classes should override these, or processor appears as an in-place pass-through */

	virtual bool can_support_io_configuration (const ChanCount& in, ChanCount& out) = 0;
	virtual ChanCount input_streams () const { return _configured_input; }
	virtual ChanCount output_streams() const { return _configured_output; }

	virtual void realtime_handle_transport_stopped () {}
	virtual void realtime_locate (bool) {}

	virtual void set_loop (Location *loc) { _loop_location = loc; }

	/* most processors won't care about this, but plugins that
	   receive MIDI or similar data from an input source that
	   may suddenly go "quiet" because of monitoring changes
	   need to know about it.
	*/
	virtual void monitoring_changed() {}

	/* note: derived classes should implement state(), NOT get_state(), to allow
	   us to merge C++ inheritance and XML lack-of-inheritance reasonably
	   smoothly.
	 */

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	virtual void set_pre_fader (bool);
	virtual bool get_pre_fader () const { return _pre_fader; }

	PBD::Signal0<void>                     ActiveChanged;
	PBD::Signal0<void>                     BypassableChanged;
	PBD::Signal2<void,ChanCount,ChanCount> ConfigurationChanged;

	/* cross-thread signals.
	 * This allows control-surfaces to show/hide a plugin GUI.
	 */
	PBD::Signal0<void> ToggleUI;
	PBD::Signal0<void> ShowUI;
	PBD::Signal0<void> HideUI;

	ProcessorWindowProxy * window_proxy () const { return _window_proxy; }
	void set_window_proxy (ProcessorWindowProxy* wp) { _window_proxy = wp; }

	PluginPinWindowProxy * pinmgr_proxy () const { return _pinmgr_proxy; }
	void set_pingmgr_proxy (PluginPinWindowProxy* wp) { _pinmgr_proxy = wp ; }

	virtual void set_owner (SessionObject*);
	SessionObject* owner() const;

protected:
	virtual XMLNode& state ();
	virtual int set_state_2X (const XMLNode&, int version);

	bool map_loop_range (samplepos_t& start, samplepos_t& end) const;

	int       _pending_active;
	bool      _active;
	bool      _next_ab_is_active;
	bool      _configured;
	ChanCount _configured_input;
	ChanCount _configured_output;
	bool      _display_to_user;
	bool      _pre_fader; ///< true if this processor is currently placed before the Amp, otherwise false
	void*     _ui_pointer;
	ProcessorWindowProxy *_window_proxy;
	PluginPinWindowProxy *_pinmgr_proxy;
	SessionObject* _owner;
	// relative to route
	samplecnt_t _input_latency;
	samplecnt_t _output_latency;
	// absolute alignment to session i/o
	samplecnt_t _capture_offset;
	samplecnt_t _playback_offset;
	Location*   _loop_location;
};

} // namespace ARDOUR

#endif /* __ardour_processor_h__ */
