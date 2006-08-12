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

    $Id$
*/

#ifndef __ardour_insert_h__
#define __ardour_insert_h__

#include <vector>
#include <string>
#include <exception>

#include <sigc++/signal.h>
#include <ardour/ardour.h>
#include <ardour/redirect.h>
#include <ardour/plugin_state.h>
#include <ardour/types.h>

class XMLNode;

namespace MIDI {
	class Port;
}

namespace ARDOUR {

class Session;
class Route;
class Plugin;

class Insert : public Redirect
{
  public:
	Insert(Session& s, Placement p);
	Insert(Session& s, string name, Placement p);

	Insert(Session& s, Placement p, int imin, int imax, int omin, int omax);
	
	virtual ~Insert() { }

	virtual void run (BufferSet& bufs, jack_nframes_t nframes, jack_nframes_t offset) = 0;
	virtual void activate () {}
	virtual void deactivate () {}

	virtual int32_t can_support_input_configuration (int32_t in) const = 0;
	virtual int32_t configure_io (int32_t magic, int32_t in, int32_t out) = 0;
	virtual int32_t compute_output_streams (int32_t cnt) const = 0;
};

class PortInsert : public Insert 
{
  public:
	PortInsert (Session&, Placement);
	PortInsert (Session&, const XMLNode&);
	PortInsert (const PortInsert&);
	~PortInsert ();

	XMLNode& state(bool full);
	XMLNode& get_state(void);
	int set_state(const XMLNode&);

	void init ();
	void run (BufferSet& bufs, jack_nframes_t nframes, jack_nframes_t offset);

	jack_nframes_t latency();
	
	ChanCount output_streams() const;
	ChanCount input_streams() const;

	int32_t can_support_input_configuration (int32_t) const;
	int32_t configure_io (int32_t magic, int32_t in, int32_t out);
	int32_t compute_output_streams (int32_t cnt) const;
};

struct PluginInsertState : public RedirectState
{
    PluginInsertState (std::string why) 
	    : RedirectState (why) {}
    ~PluginInsertState() {}

    PluginState plugin_state;
};

class PluginInsert : public Insert
{
  public:
	PluginInsert (Session&, boost::shared_ptr<Plugin>, Placement);
	PluginInsert (Session&, const XMLNode&);
	PluginInsert (const PluginInsert&);
	~PluginInsert ();

	static const string port_automation_node_name;
	
	XMLNode& state(bool);
	XMLNode& get_state(void);
	int set_state(const XMLNode&);

	StateManager::State* state_factory (std::string why) const;
	Change restore_state (StateManager::State&);

	void run (BufferSet& bufs, jack_nframes_t nframes, jack_nframes_t offset);
	void silence (jack_nframes_t nframes, jack_nframes_t offset);
	void activate ();
	void deactivate ();

	void set_block_size (jack_nframes_t nframes);

	ChanCount output_streams() const;
	ChanCount input_streams() const;
	ChanCount natural_output_streams() const;
	ChanCount natural_input_streams() const;

	int      set_count (uint32_t num);
	uint32_t get_count () const { return _plugins.size(); }

	int32_t can_support_input_configuration (int32_t) const;
	int32_t configure_io (int32_t magic, int32_t in, int32_t out);
	int32_t compute_output_streams (int32_t cnt) const;

	bool is_generator() const;

	void set_parameter (uint32_t port, float val);

	AutoState get_port_automation_state (uint32_t port);
	void set_port_automation_state (uint32_t port, AutoState);
	void protect_automation ();

	float default_parameter_value (uint32_t which);

	boost::shared_ptr<Plugin> plugin(uint32_t num=0) const {
		if (num < _plugins.size()) { 
			return _plugins[num];
		} else {
			return _plugins[0]; // we always have one
		}
	}

	PluginType type ();

	string describe_parameter (uint32_t);

	jack_nframes_t latency();

	void transport_stopped (jack_nframes_t now);
	void automation_snapshot (jack_nframes_t now);

  protected:
	void store_state (PluginInsertState&) const;

  private:

	void parameter_changed (uint32_t, float);
	
	vector<boost::shared_ptr<Plugin> > _plugins;
	void automation_run (BufferSet& bufs, jack_nframes_t nframes, jack_nframes_t offset);
	void connect_and_run (BufferSet& bufs, jack_nframes_t nframes, jack_nframes_t offset, bool with_auto, jack_nframes_t now = 0);

	void init ();
	void set_automatable ();
	void auto_state_changed (uint32_t which);
	void automation_list_creation_callback (uint32_t, AutomationList&);

	boost::shared_ptr<Plugin> plugin_factory (boost::shared_ptr<Plugin>);
};

} // namespace ARDOUR

#endif /* __ardour_insert_h__ */
