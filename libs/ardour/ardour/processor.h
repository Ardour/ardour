/*
    Copyright (C) 2009-2010 Paul Davis

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

namespace ARDOUR {

class Session;
class Route;

/** A mixer strip element - plugin, send, meter, etc */
class LIBARDOUR_API Processor : public SessionObject, public Automatable, public Latent
{
  public:
	static const std::string state_node_name;

	Processor(Session&, const std::string& name);
	Processor (const Processor& other);

	virtual ~Processor() { }

	virtual std::string display_name() const { return SessionObject::name(); }

	virtual bool display_to_user() const { return _display_to_user; }
	virtual void set_display_to_user (bool);

	bool active () const { return _pending_active; }

	virtual bool does_routing() const { return false; }

	bool get_next_ab_is_active () const { return _next_ab_is_active; }
	void set_next_ab_is_active (bool yn) { _next_ab_is_active = yn; }

	virtual framecnt_t signal_latency() const { return 0; }

	virtual int set_block_size (pframes_t /*nframes*/) { return 0; }
	virtual bool requires_fixed_sized_buffers() const { return false; }

	/** @param result_required true if, on return from this method, @a bufs is required to contain valid data;
	 *  if false, the method need not bother writing to @a bufs if it doesn't want to.
	 */
	virtual void run (BufferSet& /*bufs*/, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t /*nframes*/, bool /*result_required*/) {}
	virtual void silence (framecnt_t /*nframes*/) {}

	virtual void activate ()   { _pending_active = true; ActiveChanged(); }
	virtual void deactivate () { _pending_active = false; ActiveChanged(); }
	virtual void flush() {}

	virtual bool configure_io (ChanCount in, ChanCount out);

	/* Derived classes should override these, or processor appears as an in-place pass-through */

	virtual bool can_support_io_configuration (const ChanCount& in, ChanCount& out) = 0;
	virtual ChanCount input_streams () const { return _configured_input; }
	virtual ChanCount output_streams() const { return _configured_output; }

	virtual void realtime_handle_transport_stopped () {}
	virtual void realtime_locate () {}

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

	virtual XMLNode& state (bool full);
	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	void set_pre_fader (bool);

	PBD::Signal0<void>                     ActiveChanged;
	PBD::Signal2<void,ChanCount,ChanCount> ConfigurationChanged;

	void  set_ui (void*);
	void* get_ui () const { return _ui_pointer; }

        void set_owner (SessionObject*);
        SessionObject* owner() const;

protected:
	virtual int set_state_2X (const XMLNode&, int version);

	int       _pending_active;
	bool      _active;
	bool      _next_ab_is_active;
	bool      _configured;
	ChanCount _configured_input;
	ChanCount _configured_output;
	bool      _display_to_user;
	bool      _pre_fader; ///< true if this processor is currently placed before the Amp, otherwise false
	void*     _ui_pointer;
        SessionObject* _owner;
};

} // namespace ARDOUR

#endif /* __ardour_processor_h__ */
