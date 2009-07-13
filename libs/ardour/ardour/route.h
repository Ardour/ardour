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
#include <cstring>
#include <list>
#include <map>
#include <set>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <glibmm/thread.h>
#include "pbd/fastlog.h"
#include "pbd/xml++.h"
#include "pbd/undo.h"
#include "pbd/stateful.h" 
#include "pbd/controllable.h"
#include "pbd/destructible.h"

#include "ardour/ardour.h"
#include "ardour/io.h"
#include "ardour/types.h"
#include "ardour/mute_master.h"

namespace ARDOUR {

class Amp;
class Delivery;
class IOProcessor;
class Panner;
class Processor;
class RouteGroup;
class Send;
class InternalReturn;

class Route : public SessionObject, public AutomatableControls
{
  public:

	typedef std::list<boost::shared_ptr<Processor> > ProcessorList;

	enum Flag {
		Hidden = 0x1,
		MasterOut = 0x2,
		ControlOut = 0x4
	};

	Route (Session&, std::string name, Flag flags = Flag(0),
	       DataType default_type = DataType::AUDIO);
	Route (Session&, const XMLNode&, DataType default_type = DataType::AUDIO);
	virtual ~Route();

	boost::shared_ptr<IO> input() const { return _input; }
	boost::shared_ptr<IO> output() const { return _output; }

	ChanCount n_inputs() const { return _input->n_ports(); }
	ChanCount n_outputs() const { return _output->n_ports(); }

	bool active() const { return _active; }
	void set_active (bool yn);


	static std::string ensure_track_or_route_name(std::string, Session &);

	std::string comment() { return _comment; }
	void set_comment (std::string str, void *src);

	bool set_name (const std::string& str);

	long order_key (std::string const &) const;
	void set_order_key (std::string const &, long);

	bool is_hidden() const { return _flags & Hidden; }
	bool is_master() const { return _flags & MasterOut; }
	bool is_control() const { return _flags & ControlOut; }

	/* these are the core of the API of a Route. see the protected sections as well */

	virtual int roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame, 
			  int declick, bool can_record, bool rec_monitors_input);

	virtual int no_roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame, 
			     bool state_changing, bool can_record, bool rec_monitors_input);

	virtual int silent_roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame, 
				 bool can_record, bool rec_monitors_input);

	virtual void toggle_monitor_input ();
	virtual bool can_record() { return false; }

	virtual void set_record_enable (bool yn, void *src) {}
	virtual bool record_enabled() const { return false; }
	virtual void handle_transport_stopped (bool abort, bool did_locate, bool flush_processors);
	virtual void set_pending_declick (int);

	/* end of vfunc-based API */

	void shift (nframes64_t, nframes64_t);
	
	void set_gain (gain_t val, void *src);
	void inc_gain (gain_t delta, void *src);

	void set_mute (bool yn, void* src);
	bool muted () const;


	/* controls use set_solo() to modify this route's solo state
	 */

	void set_solo (bool yn, void *src);
	bool soloed () const { return (bool) _solo_level; }

	void set_solo_isolated (bool yn, void *src);
	bool solo_isolated() const;

	void set_listen (bool yn, void* src);
	bool listening () const;
	
	void set_phase_invert (bool yn);
	bool phase_invert() const;

	void set_denormal_protection (bool yn);
	bool denormal_protection() const;

	void       set_route_group (RouteGroup *, void *);
	void       drop_route_group (void *);
	RouteGroup *route_group () const { return _route_group; }

	virtual void set_meter_point (MeterPoint, void *src);
	MeterPoint   meter_point() const { return _meter_point; }
	void         meter ();

	/* Processors */

	boost::shared_ptr<Amp> amp() const  { return _amp; }
	PeakMeter&       peak_meter()       { return *_meter.get(); }
	const PeakMeter& peak_meter() const { return *_meter.get(); }
	boost::shared_ptr<PeakMeter> shared_peak_meter() const { return _meter; }

	void flush_processors ();

	void foreach_processor (sigc::slot<void, boost::weak_ptr<Processor> > method) {
		Glib::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			method (boost::weak_ptr<Processor> (*i));
		}
	}
	
	boost::shared_ptr<Processor> nth_processor (uint32_t n) {
		Glib::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i;
		for (i = _processors.begin(); i != _processors.end() && n; ++i, --n) {}
		if (i == _processors.end()) {
			return boost::shared_ptr<Processor> ();
		} else {
			return *i;
		}
	}

	bool processor_is_prefader (boost::shared_ptr<Processor> p);

	ChanCount max_processor_streams () const { return processor_max_streams; }

	/* special processors */

	boost::shared_ptr<Delivery>       control_outs() const { return _control_outs; }
	boost::shared_ptr<Delivery>       main_outs() const { return _main_outs; }
	boost::shared_ptr<InternalReturn> internal_return() const { return _intreturn; }
	boost::shared_ptr<Send>           internal_send_for (boost::shared_ptr<const Route> target) const;
	void add_internal_return ();
	BufferSet* get_return_buffer () const;
	void release_return_buffer () const;
	void put_control_outs_at (Placement);

	/** A record of the stream configuration at some point in the processor list.
	 * Used to return where and why an processor list configuration request failed.
	 */
	struct ProcessorStreams {
		ProcessorStreams(size_t i=0, ChanCount c=ChanCount()) : index(i), count(c) {}

		uint32_t  index; ///< Index of processor where configuration failed
		ChanCount count; ///< Input requested of processor
	};

	int add_processor (boost::shared_ptr<Processor>, Placement placement, ProcessorStreams* err = 0);
	int add_processor (boost::shared_ptr<Processor>, ProcessorList::iterator iter, ProcessorStreams* err = 0);
	int add_processors (const ProcessorList&, Placement placement, ProcessorStreams* err = 0);
	int add_processors (const ProcessorList&, ProcessorList::iterator iter, ProcessorStreams* err = 0);
	int remove_processor (boost::shared_ptr<Processor>, ProcessorStreams* err = 0);
	int reorder_processors (const ProcessorList& new_order, ProcessorStreams* err = 0);
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

	sigc::signal<void>       active_changed;
	sigc::signal<void>       phase_invert_changed;
	sigc::signal<void>       denormal_protection_changed;
	sigc::signal<void,void*> listen_changed;
	sigc::signal<void,void*> solo_changed;
	sigc::signal<void,void*> solo_safe_changed;
	sigc::signal<void,void*> solo_isolated_changed;
	sigc::signal<void,void*> comment_changed;
	sigc::signal<void,void*> mute_changed;
	sigc::signal<void,void*> pre_fader_changed;
	sigc::signal<void,void*> post_fader_changed;
	sigc::signal<void,void*> control_outs_changed;
	sigc::signal<void,void*> main_outs_changed;
	sigc::signal<void>       processors_changed;
	sigc::signal<void,void*> record_enable_changed;
	sigc::signal<void,void*> route_group_changed;
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
	virtual void set_processor_state (const XMLNode&);
	
	int save_as_template (const std::string& path, const std::string& name);

	sigc::signal<void,void*> SelectedChanged;
	
	int listen_via (boost::shared_ptr<Route>, bool);
	void drop_listen (boost::shared_ptr<Route>);

	bool feeds (boost::shared_ptr<Route>);
	std::set<boost::shared_ptr<Route> > fed_by;

	/* Controls (not all directly owned by the Route */

	boost::shared_ptr<AutomationControl> get_control (const Evoral::Parameter& param);

	struct SoloControllable : public AutomationControl {
		SoloControllable (std::string name, Route&);
		void set_value (float);
		float get_value (void) const;
		
		Route& route;
	};

	boost::shared_ptr<AutomationControl> solo_control() const {
		return _solo_control;
	}

	boost::shared_ptr<AutomationControl> mute_control() const {
		return _mute_master;
	}

	boost::shared_ptr<MuteMaster> mute_master() const { 
		return _mute_master; 
	}

	/* Route doesn't own these items, but sub-objects that it does own have them
	   and to make UI code a bit simpler, we provide direct access to them
	   here.
	*/

	boost::shared_ptr<Panner> panner() const;
	boost::shared_ptr<AutomationControl> gain_control() const;

	void automation_snapshot (nframes_t now, bool force=false);
	void protect_automation ();
	
	void set_remote_control_id (uint32_t id);
	uint32_t remote_control_id () const;
	sigc::signal<void> RemoteControlIDChanged;

	void sync_order_keys (std::string const &);
	static sigc::signal<void, std::string const &> SyncOrderKeys;

  protected:
	friend class Session;

	void catch_up_on_solo_mute_override ();
	void mod_solo_level (int32_t);
	uint32_t solo_level () const { return _solo_level; }
	void set_block_size (nframes_t nframes);
	bool has_external_redirects() const;
	void curve_reallocate ();
	void just_meter_input (sframes_t start_frame, sframes_t end_frame, nframes_t nframes);

  protected:
	nframes_t check_initial_delay (nframes_t, nframes_t&);
	
	void passthru (sframes_t start_frame, sframes_t end_frame,
		       nframes_t nframes, int declick);

	virtual void process_output_buffers (BufferSet& bufs,
					     sframes_t start_frame, sframes_t end_frame,
					     nframes_t nframes, bool with_processors, int declick);
	
	boost::shared_ptr<IO> _input;
	boost::shared_ptr<IO> _output;

	bool           _active;
	nframes_t      _initial_delay;
	nframes_t      _roll_delay;

	ProcessorList  _processors;
	mutable Glib::RWLock   _processor_lock;
	boost::shared_ptr<Delivery> _main_outs;
	boost::shared_ptr<Delivery> _control_outs;
	boost::shared_ptr<InternalReturn> _intreturn;

	Flag           _flags;
	int            _pending_declick;
	MeterPoint     _meter_point;
	uint32_t       _phase_invert;
	uint32_t       _solo_level;
	bool           _solo_isolated;

	bool           _denormal_protection;
	
	bool _recordable : 1;
	bool _silent : 1;
	bool _declickable : 1;

	boost::shared_ptr<SoloControllable> _solo_control;
	boost::shared_ptr<MuteMaster> _mute_master;

	RouteGroup*    _route_group;
	std::string    _comment;
	bool           _have_internal_generator;
	bool           _solo_safe;
	DataType       _default_type;

  protected:

	virtual XMLNode& state(bool);

	void passthru_silence (sframes_t start_frame, sframes_t end_frame,
	                       nframes_t nframes, int declick);
	
	void silence (nframes_t nframes);
	
	sigc::connection input_signal_connection;

	ChanCount processor_max_streams;
	uint32_t _remote_control_id;

	uint32_t pans_required() const;
	ChanCount n_process_buffers ();
	
	virtual int  _set_state (const XMLNode&, bool call_base);

	boost::shared_ptr<Amp>       _amp;
	boost::shared_ptr<PeakMeter> _meter;
	sigc::connection _meter_connection;

  private:
	void init ();

	static uint32_t order_key_cnt;

	typedef std::map<std::string, long> OrderKeys;
	OrderKeys order_keys;

	void input_change_handler (IOChange, void *src);
	void output_change_handler (IOChange, void *src);

	bool _in_configure_processors;

	int configure_processors (ProcessorStreams*);
	int configure_processors_unlocked (ProcessorStreams*);

	bool add_processor_from_xml (const XMLNode&, ProcessorList::iterator iter);	

	void placement_range (Placement p, ProcessorList::iterator& start, ProcessorList::iterator& end);
};

} // namespace ARDOUR

#endif /* __ardour_route_h__ */
