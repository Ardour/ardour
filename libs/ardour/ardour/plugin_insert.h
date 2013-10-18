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

#include "ardour/ardour.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/processor.h"
#include "ardour/automation_control.h"

class XMLNode;

namespace ARDOUR {

class Session;
class Route;
class Plugin;

/** Plugin inserts: send data through a plugin
 */
class LIBARDOUR_API PluginInsert : public Processor
{
  public:
	PluginInsert (Session&, boost::shared_ptr<Plugin> = boost::shared_ptr<Plugin>());
	~PluginInsert ();

	static const std::string port_automation_node_name;

	XMLNode& state(bool);
	XMLNode& get_state(void);
	int set_state(const XMLNode&, int version);

	void run (BufferSet& in, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool);
	void silence (framecnt_t nframes);

	void activate ();
	void deactivate ();
	void flush ();

	int set_block_size (pframes_t nframes);

	ChanCount output_streams() const;
	ChanCount input_streams() const;
	ChanCount natural_output_streams() const;
	ChanCount natural_input_streams() const;

	bool     set_count (uint32_t num);
	uint32_t get_count () const { return _plugins.size(); }

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	bool has_no_inputs() const;
	bool has_no_audio_inputs() const;
	bool is_midi_instrument() const;

	void realtime_handle_transport_stopped ();
	void realtime_locate ();
	void monitoring_changed ();

	struct PluginControl : public AutomationControl
	{
		PluginControl (PluginInsert* p, const Evoral::Parameter &param,
				boost::shared_ptr<AutomationList> list = boost::shared_ptr<AutomationList>());

		void set_value (double val);
		double get_value (void) const;
		XMLNode& get_state();

		double internal_to_interface (double) const;
		double interface_to_internal (double) const;

	private:
		PluginInsert* _plugin;
		bool _logarithmic;
		bool _sr_dependent;
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

	std::string describe_parameter (Evoral::Parameter param);

	framecnt_t signal_latency () const;

	boost::shared_ptr<Plugin> get_impulse_analysis_plugin();

	void collect_signal_for_analysis (framecnt_t nframes);

	bool splitting () const {
		return _match.method == Split;
	}

	PBD::Signal2<void,BufferSet*, BufferSet*> AnalysisDataGathered;
	/** Emitted when the return value of splitting () has changed */
	PBD::Signal0<void> SplittingChanged;

	/** Enumeration of the ways in which we can match our insert's
	 *  IO to that of the plugin(s).
	 */
	enum MatchingMethod {
		Impossible,  ///< we can't
		Delegate,    ///< we are delegating to the plugin, and it can handle it
		NoInputs,    ///< plugin has no inputs, so anything goes
		ExactMatch,  ///< our insert's inputs are the same as the plugin's
		Replicate,   ///< we have multiple instances of the plugin
		Split,       ///< we copy one of our insert's inputs to multiple plugin inputs
		Hide,        ///< we `hide' some of the plugin's inputs by feeding them silence
	};

  private:
	/* disallow copy construction */
	PluginInsert (const PluginInsert&);

	void parameter_changed (uint32_t, float);

	void  set_parameter (Evoral::Parameter param, float val);
	float get_parameter (Evoral::Parameter param);

	float default_parameter_value (const Evoral::Parameter& param);

	typedef std::vector<boost::shared_ptr<Plugin> > Plugins;
	Plugins _plugins;

	boost::weak_ptr<Plugin> _impulseAnalysisPlugin;

	framecnt_t _signal_analysis_collected_nframes;
	framecnt_t _signal_analysis_collect_nframes_max;

	BufferSet _signal_analysis_inputs;
	BufferSet _signal_analysis_outputs;

	ChanCount midi_bypass;

	/** Description of how we can match our plugin's IO to our own insert IO */
	struct Match {
		Match () : method (Impossible), plugins (0) {}
		Match (MatchingMethod m, int32_t p, ChanCount h = ChanCount ()) : method (m), plugins (p), hide (h) {}
		
		MatchingMethod method; ///< method to employ
		int32_t plugins;       ///< number of copies of the plugin that we need
		ChanCount hide;        ///< number of channels to hide
	};

	Match private_can_support_io_configuration (ChanCount const &, ChanCount &);

	/** details of the match currently being used */
	Match _match;

	void automation_run (BufferSet& bufs, pframes_t nframes);
	void connect_and_run (BufferSet& bufs, pframes_t nframes, framecnt_t offset, bool with_auto, framepos_t now = 0);

	void create_automatable_parameters ();
	void control_list_automation_state_changed (Evoral::Parameter, AutoState);
	void set_parameter_state_2X (const XMLNode& node, int version);
	void set_control_ids (const XMLNode&, int version);

	boost::shared_ptr<Plugin> plugin_factory (boost::shared_ptr<Plugin>);
	void add_plugin (boost::shared_ptr<Plugin>);

        void start_touch (uint32_t param_id);
        void end_touch (uint32_t param_id);
};

} // namespace ARDOUR

#endif /* __ardour_plugin_insert_h__ */
