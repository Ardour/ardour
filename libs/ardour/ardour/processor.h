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

#ifndef __ardour_processor_h__
#define __ardour_processor_h__

#include <vector>
#include <string>
#include <exception>

#include <pbd/statefuldestructible.h> 

#include <sigc++/signal.h>

#include <ardour/types.h>
#include <ardour/ardour.h>
#include <ardour/plugin_state.h>
#include <ardour/buffer_set.h>
#include <ardour/automatable.h>


class XMLNode;

namespace ARDOUR {

class Session;

/* A mixer strip element - plugin, send, meter, etc.
 */
class Processor : public Automatable
{
  public:
	static const string state_node_name;

	Processor(Session&, const string& name, Placement p); // TODO: remove placement in favour of sort key
	
	virtual ~Processor() { }
	
	static boost::shared_ptr<Processor> clone (boost::shared_ptr<const Processor>);

	uint32_t sort_key() const { return _sort_key; }
	void set_sort_key (uint32_t key);

	Placement placement() const { return _placement; }
	void set_placement (Placement);
	
	bool active () const { return _active; }
	void set_active (bool yn);
	
	bool get_next_ab_is_active () const { return _next_ab_is_active; }
	void set_next_ab_is_active (bool yn) { _next_ab_is_active = yn; }
	
	virtual nframes_t latency() { return 0; }
	
	virtual void transport_stopped (nframes_t frame) {}
	
	virtual void set_block_size (nframes_t nframes) {}

	virtual void run (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset) = 0;
	virtual void silence (nframes_t nframes, nframes_t offset) {}
	
	virtual void activate () { _active = true; ActiveChanged.emit(); }
	virtual void deactivate () { _active = false; ActiveChanged.emit(); }
	
	virtual bool configure_io (ChanCount in, ChanCount out) { _configured_input = in; return (_configured = true); }

	/* Act as a pass through, if not overridden */
	virtual bool      can_support_input_configuration (ChanCount in) const { return true; }
	virtual ChanCount output_for_input_configuration (ChanCount in) const { return in; }
	virtual ChanCount output_streams() const { return _configured_input; }
	virtual ChanCount input_streams () const { return _configured_input; }

	virtual XMLNode& state (bool full);
	virtual XMLNode& get_state (void);
	virtual int set_state (const XMLNode&);
	
	void *get_gui () const { return _gui; }
	void  set_gui (void *p) { _gui = p; }

	static sigc::signal<void,Processor*> ProcessorCreated;

	sigc::signal<void> ActiveChanged;
	sigc::signal<void> PlacementChanged;

protected:
	bool      _active;
	bool      _next_ab_is_active;
	bool      _configured;
	ChanCount _configured_input;
	Placement _placement;
	uint32_t  _sort_key;
	void*     _gui;  /* generic, we don't know or care what this is */
};

} // namespace ARDOUR

#endif /* __ardour_processor_h__ */
