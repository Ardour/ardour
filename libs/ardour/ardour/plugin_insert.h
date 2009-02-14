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

#ifndef __ardour_plugin_insert_h__
#define __ardour_plugin_insert_h__

#include <vector>
#include <string>

#include <boost/weak_ptr.hpp>

#include <sigc++/signal.h>
#include <ardour/ardour.h>
#include <ardour/types.h>
#include <ardour/processor.h>
#include <ardour/automation_control.h>

class XMLNode;

namespace ARDOUR {

class Session;
class Route;
class Plugin;

/** Plugin inserts: send data through a plugin
 */
class PluginInsert : public Processor
{
  public:
	PluginInsert (Session&, boost::shared_ptr<Plugin>, Placement);
	PluginInsert (Session&, const XMLNode&);
	~PluginInsert ();

	static const string port_automation_node_name;
	
	XMLNode& state(bool);
	XMLNode& get_state(void);
	int set_state(const XMLNode&);

	void run_in_place (BufferSet& in, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset);
	void silence (nframes_t nframes, nframes_t offset);
	
	void activate ();
	void deactivate ();

	void set_block_size (nframes_t nframes);

	ChanCount output_streams() const;
	ChanCount input_streams() const;
	ChanCount natural_output_streams() const;
	ChanCount natural_input_streams() const;

	bool     set_count (uint32_t num);
	uint32_t get_count () const { return _plugins.size(); }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) const;
	bool configure_io (ChanCount in, ChanCount out);

	bool is_generator() const;

	struct PluginControl : public AutomationControl 
	{
 	    PluginControl (PluginInsert* p, const Evoral::Parameter &param,
 		    boost::shared_ptr<AutomationList> list = boost::shared_ptr<AutomationList>());
	    
		void set_value (float val);
	    float get_value (void) const;
	
	  private:
 		PluginInsert* _plugin;
		bool _logarithmic;
		bool _toggled;
	};

	boost::shared_ptr<Plugin> plugin(uint32_t num=0) const {
		if (num < _plugins.size()) { 
			return _plugins[num];
		} else {
			return _plugins[0]; // we always have one
		}
	}

	PluginType type ();

	string describe_parameter (Evoral::Parameter param);

	nframes_t signal_latency() const;

	boost::shared_ptr<Plugin> get_impulse_analysis_plugin();

	sigc::signal<void, BufferSet*, BufferSet*> AnalysisDataGathered;
	void collect_signal_for_analysis(nframes_t nframes) { 
		// called from outside the audio thread, so this should be safe
		_signal_analysis_input_bufferset.ensure_buffers(input_streams(), nframes);
		_signal_analysis_output_bufferset.ensure_buffers(output_streams(), nframes);

		_signal_analysis_collect_nframes_max = nframes; 
		_signal_analysis_collected_nframes   = 0;
	}

  private:
	/* disallow copy construction */
	PluginInsert (const PluginInsert&);

	void parameter_changed (Evoral::Parameter, float);
	
	void  set_parameter (Evoral::Parameter param, float val);
	float get_parameter (Evoral::Parameter param);

	float default_parameter_value (const Evoral::Parameter& param);
	
	std::vector<boost::shared_ptr<Plugin> > _plugins;

	boost::weak_ptr<Plugin> _impulseAnalysisPlugin;

	nframes_t _signal_analysis_collected_nframes;
	nframes_t _signal_analysis_collect_nframes_max;

	BufferSet _signal_analysis_input_bufferset;
	BufferSet _signal_analysis_output_bufferset;
	
	void automation_run (BufferSet& bufs, nframes_t nframes, nframes_t offset);
	void connect_and_run (BufferSet& bufs, nframes_t nframes, nframes_t offset, bool with_auto, nframes_t now = 0);

	void init ();
	void set_automatable ();
	void auto_state_changed (Evoral::Parameter which);

	int32_t count_for_configuration (ChanCount in, ChanCount out) const;

	boost::shared_ptr<Plugin> plugin_factory (boost::shared_ptr<Plugin>);
};

} // namespace ARDOUR

#endif /* __ardour_plugin_insert_h__ */
