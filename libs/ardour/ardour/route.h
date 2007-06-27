/*
    Copyright (C) 2000-2002 Paul Davis 

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

#ifndef __ardour_route_h__
#define __ardour_route_h__

#include <cmath>
#include <list>
#include <set>
#include <map>
#include <string>

#include <boost/shared_ptr.hpp>

#include <pbd/fastlog.h>
#include <glibmm/thread.h>
#include <pbd/xml++.h>
#include <pbd/undo.h>
#include <pbd/stateful.h> 
#include <pbd/controllable.h>
#include <pbd/destructible.h>

#include <ardour/ardour.h>
#include <ardour/io.h>
#include <ardour/session.h>
#include <ardour/io_processor.h>
#include <ardour/types.h>

namespace ARDOUR {

class Processor;
class Send;
class RouteGroup;

enum mute_type {
    PRE_FADER =    0x1,
    POST_FADER =   0x2,
    CONTROL_OUTS = 0x4,
    MAIN_OUTS =    0x8
};

class Route : public IO
{
  protected:

	typedef list<boost::shared_ptr<Processor> > ProcessorList;

  public:

	enum Flag {
		Hidden = 0x1,
		MasterOut = 0x2,
		ControlOut = 0x4
	};


	Route (Session&, std::string name, int input_min, int input_max, int output_min, int output_max,
	       Flag flags = Flag(0), DataType default_type = DataType::AUDIO);
	Route (Session&, const XMLNode&, DataType default_type = DataType::AUDIO);
	virtual ~Route();

	std::string comment() { return _comment; }
	void set_comment (std::string str, void *src);

	long order_key (const char* name) const;
	void set_order_key (const char* name, long n);

	bool hidden() const { return _flags & Hidden; }
	bool master() const { return _flags & MasterOut; }
	bool control() const { return _flags & ControlOut; }

	/* these are the core of the API of a Route. see the protected sections as well */


	virtual int  roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
			   nframes_t offset, int declick, bool can_record, bool rec_monitors_input);

	virtual int  no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
			      nframes_t offset, bool state_changing, bool can_record, bool rec_monitors_input);

	virtual int  silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
				  nframes_t offset, bool can_record, bool rec_monitors_input);
	virtual void toggle_monitor_input ();
	virtual bool can_record() { return false; }
	virtual void set_record_enable (bool yn, void *src) {}
	virtual bool record_enabled() const { return false; }
	virtual void handle_transport_stopped (bool abort, bool did_locate, bool flush_processors);
	virtual void set_pending_declick (int);

	/* end of vfunc-based API */

	/* override IO::set_gain() to provide group control */

	void set_gain (gain_t val, void *src);
	void inc_gain (gain_t delta, void *src);

	bool active() const { return _active; }
	void set_active (bool yn);

	void set_solo (bool yn, void *src);
	bool soloed() const { return _soloed; }

	void set_solo_safe (bool yn, void *src);
	bool solo_safe() const { return _solo_safe; }

	void set_mute (bool yn, void *src);
	bool muted() const { return _muted; }
	bool solo_muted() const { return desired_solo_gain == 0.0; }

	void set_mute_config (mute_type, bool, void *src);
	bool get_mute_config (mute_type);

	void       set_edit_group (RouteGroup *, void *);
	void       drop_edit_group (void *);
	RouteGroup *edit_group () { return _edit_group; }

	void       set_mix_group (RouteGroup *, void *);
	void       drop_mix_group (void *);
	RouteGroup *mix_group () { return _mix_group; }

	virtual void  set_meter_point (MeterPoint, void *src);
	MeterPoint  meter_point() const { return _meter_point; }

	/* Processors */

	void flush_processors ();

	template<class T> void foreach_processor (T *obj, void (T::*func)(boost::shared_ptr<Processor>)) {
		Glib::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			(obj->*func) (*i);
		}
	}

	boost::shared_ptr<Processor> nth_processor (uint32_t n) {
		Glib::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i;
		for (i = _processors.begin(); i != _processors.end() && n; ++i, --n);
		if (i == _processors.end()) {
			return boost::shared_ptr<IOProcessor> ();
		} else {
			return *i;
		}
	}
	
	ChanCount max_processor_outs () const { return processor_max_outs; }
	ChanCount pre_fader_streams() const;
	
	/** A record of the stream configuration at some point in the processor list.
	 * Used to return where and why an processor list configuration request failed.
	 */
	struct ProcessorStreams {
		ProcessorStreams(size_t i=0, ChanCount c=ChanCount()) : index(i), count(c) {}

		size_t    index; ///< Index of processor where configuration failed
		ChanCount count; ///< Input requested of processor
	};

	int add_processor (boost::shared_ptr<Processor>, ProcessorStreams* err = 0);
	int add_processors (const ProcessorList&, ProcessorStreams* err = 0);
	int remove_processor (boost::shared_ptr<Processor>, ProcessorStreams* err = 0);
	int copy_processors (const Route&, Placement, ProcessorStreams* err = 0);
	int sort_processors (ProcessorStreams* err = 0);
	void disable_processors (Placement);
	void disable_processors ();
	void disable_plugins (Placement);
	void disable_plugins ();
	void ab_plugins (bool forward);
	void clear_processors (Placement);
	void all_processors_flip();
	void all_processors_active (Placement, bool state);

	virtual nframes_t update_total_latency();
	void set_latency_delay (nframes_t);
	void set_user_latency (nframes_t);
	nframes_t initial_delay() const { return _initial_delay; }

	sigc::signal<void,void*> solo_changed;
	sigc::signal<void,void*> solo_safe_changed;
	sigc::signal<void,void*> comment_changed;
	sigc::signal<void,void*> mute_changed;
	sigc::signal<void,void*> pre_fader_changed;
	sigc::signal<void,void*> post_fader_changed;
	sigc::signal<void,void*> control_outs_changed;
	sigc::signal<void,void*> main_outs_changed;
	sigc::signal<void>       processors_changed;
	sigc::signal<void,void*> record_enable_changed;
	sigc::signal<void,void*> edit_group_changed;
	sigc::signal<void,void*> mix_group_changed;
	sigc::signal<void>       active_changed;
	sigc::signal<void,void*> meter_change;
	sigc::signal<void>       signal_latency_changed;
	sigc::signal<void>       initial_delay_changed;

	/* gui's call this for their own purposes. */

	sigc::signal<void,std::string,void*> gui_changed;

	/* stateful */

	XMLNode& get_state();
	int set_state(const XMLNode& node);
	virtual XMLNode& get_template();

	XMLNode& get_processor_state ();
	int set_processor_state (const XMLNode&);

	sigc::signal<void,void*> SelectedChanged;

	int set_control_outs (const vector<std::string>& ports);
	IO* control_outs() { return _control_outs; }

	bool feeds (boost::shared_ptr<Route>);
	set<boost::shared_ptr<Route> > fed_by;

	struct ToggleControllable : public PBD::Controllable {
	    enum ToggleType {
		    MuteControl = 0,
		    SoloControl
	    };
	    
	    ToggleControllable (std::string name, Route&, ToggleType);
	    void set_value (float);
	    float get_value (void) const;

	    Route& route;
	    ToggleType type;
	};

	PBD::Controllable& solo_control() {
		return _solo_control;
	}

	PBD::Controllable& mute_control() {
		return _mute_control;
	}
	
	void automation_snapshot (nframes_t now);
	void protect_automation ();
	
	void set_remote_control_id (uint32_t id);
	uint32_t remote_control_id () const;
	sigc::signal<void> RemoteControlIDChanged;

  protected:
	friend class Session;

	void set_solo_mute (bool yn);
	void set_block_size (nframes_t nframes);
	bool has_external_redirects() const;
	void curve_reallocate ();

  protected:
	Flag _flags;

	/* tight cache-line access here is more important than sheer speed of
	   access.
	*/

	bool                     _muted : 1;
	bool                     _soloed : 1;
	bool                     _solo_safe : 1;
	bool                     _recordable : 1;
	bool                     _active : 1;
	bool                     _mute_affects_pre_fader : 1;
	bool                     _mute_affects_post_fader : 1;
	bool                     _mute_affects_control_outs : 1;
	bool                     _mute_affects_main_outs : 1;
	bool                     _silent : 1;
	bool                     _declickable : 1;
	int                      _pending_declick;
	
	MeterPoint               _meter_point;

	gain_t                    solo_gain;
	gain_t                    mute_gain;
	gain_t                    desired_solo_gain;
	gain_t                    desired_mute_gain;



	nframes_t           _initial_delay;
	nframes_t           _roll_delay;
	ProcessorList       _processors;
	Glib::RWLock        _processor_lock;
	IO                 *_control_outs;
	Glib::Mutex         _control_outs_lock;
	RouteGroup         *_edit_group;
	RouteGroup         *_mix_group;
	std::string         _comment;
	bool                _have_internal_generator;

	ToggleControllable _solo_control;
	ToggleControllable _mute_control;
	
	nframes_t check_initial_delay (nframes_t, nframes_t&, nframes_t&);
	
	void passthru (nframes_t start_frame, nframes_t end_frame, 
		       nframes_t nframes, nframes_t offset, int declick, bool meter_inputs);

	virtual void process_output_buffers (BufferSet& bufs,
				     nframes_t start_frame, nframes_t end_frame,
				     nframes_t nframes, nframes_t offset, bool with_processors, int declick,
				     bool meter);

  protected:

	virtual XMLNode& state(bool);

	void passthru_silence (nframes_t start_frame, nframes_t end_frame,
	                       nframes_t nframes, nframes_t offset, int declick,
	                       bool meter);
	
	void silence (nframes_t nframes, nframes_t offset);
	
	sigc::connection input_signal_connection;

	ChanCount processor_max_outs;
	uint32_t _remote_control_id;

	uint32_t pans_required() const;
	ChanCount n_process_buffers ();

	virtual int  _set_state (const XMLNode&, bool call_base);
	virtual void _set_processor_states (const XMLNodeList&);

  private:
	void init ();

	static uint32_t order_key_cnt;

	struct ltstr
	{
	    bool operator()(const char* s1, const char* s2) const
	    {
		    return strcmp(s1, s2) < 0;
	    }
	};

	typedef std::map<const char*,long,ltstr> OrderKeys;
	OrderKeys order_keys;

	void input_change_handler (IOChange, void *src);
	void output_change_handler (IOChange, void *src);

	int reset_plugin_counts (ProcessorStreams*); /* locked */
	int _reset_plugin_counts (ProcessorStreams*); /* unlocked */

	/* processor I/O channels and plugin count handling */

	struct ProcessorCount {
	    boost::shared_ptr<ARDOUR::Processor> processor;
	    ChanCount in;
	    ChanCount out;

	    ProcessorCount (boost::shared_ptr<ARDOUR::Processor> ins) : processor(ins) {}
	};
	
	int32_t apply_some_plugin_counts (std::list<ProcessorCount>& iclist);
	bool    check_some_plugin_counts (std::list<ProcessorCount>& iclist, ChanCount required_inputs, ProcessorStreams* err_streams);

	void set_deferred_state ();
	void add_processor_from_xml (const XMLNode&);
};

} // namespace ARDOUR

#endif /* __ardour_route_h__ */
