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
#include <boost/dynamic_bitset.hpp>

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
#include "ardour/route_group_member.h"
#include "ardour/graphnode.h"
#include "ardour/automatable.h"

namespace ARDOUR {

class Amp;
class Delivery;
class IOProcessor;
class Panner;
class Processor;
class RouteGroup;
class Send;
class InternalReturn;
class MonitorProcessor;

class Route : public SessionObject, public Automatable, public RouteGroupMember, public GraphNode
{
  public:

	typedef std::list<boost::shared_ptr<Processor> > ProcessorList;

	enum Flag {
		Hidden = 0x1,
		MasterOut = 0x2,
		MonitorOut = 0x4
	};

	Route (Session&, std::string name, Flag flags = Flag(0), DataType default_type = DataType::AUDIO);
	virtual ~Route();

	virtual int init ();

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

	int32_t order_key (std::string const &) const;
	void set_order_key (std::string const &, int32_t);

	bool is_hidden() const { return _flags & Hidden; }
	bool is_master() const { return _flags & MasterOut; }
	bool is_monitor() const { return _flags & MonitorOut; }

	/* these are the core of the API of a Route. see the protected sections as well */

	virtual int roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame,
                          int declick, bool can_record, bool rec_monitors_input, bool& need_butler);

	virtual int no_roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame,
			bool state_changing, bool can_record, bool rec_monitors_input);

	virtual int silent_roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame,
                                 bool can_record, bool rec_monitors_input, bool& need_butler);

	virtual void toggle_monitor_input ();
	virtual bool can_record() { return false; }

	virtual void set_record_enabled (bool /*yn*/, void * /*src*/) {}
	virtual bool record_enabled() const { return false; }
	virtual void handle_transport_stopped (bool abort, bool did_locate, bool flush_processors);
	virtual void set_pending_declick (int);

	/* end of vfunc-based API */

	void shift (nframes64_t, nframes64_t);

	void set_gain (gain_t val, void *src);
	void inc_gain (gain_t delta, void *src);

	void set_mute_points (MuteMaster::MutePoint);
	MuteMaster::MutePoint mute_points () const;

	bool muted () const;
	void set_mute (bool yn, void* src);

	/* controls use set_solo() to modify this route's solo state
	 */

	void set_solo (bool yn, void *src);
	bool soloed () const { return self_soloed () || soloed_by_others (); }

        bool soloed_by_others () const { return _soloed_by_others_upstream||_soloed_by_others_downstream; }
	bool soloed_by_others_upstream () const { return _soloed_by_others_upstream; }
	bool soloed_by_others_downstream () const { return _soloed_by_others_downstream; }
	bool self_soloed () const { return _self_solo; }
	
	void set_solo_isolated (bool yn, void *src);
	bool solo_isolated() const;

	void set_solo_safe (bool yn, void *src);
	bool solo_safe() const;

	void set_listen (bool yn, void* src);
	bool listening () const;

	void set_phase_invert (uint32_t, bool yn);
	void set_phase_invert (boost::dynamic_bitset<>);
	bool phase_invert (uint32_t) const;
	boost::dynamic_bitset<> phase_invert () const;

	void set_denormal_protection (bool yn);
	bool denormal_protection() const;

	void         set_meter_point (MeterPoint);
	void         infer_meter_point () const;
	MeterPoint   meter_point() const { return _meter_point; }
	void         meter ();

	/* Processors */

	boost::shared_ptr<Amp> amp() const  { return _amp; }
	PeakMeter&       peak_meter()       { return *_meter.get(); }
	const PeakMeter& peak_meter() const { return *_meter.get(); }
	boost::shared_ptr<PeakMeter> shared_peak_meter() const { return _meter; }

	void flush_processors ();

	void foreach_processor (boost::function<void(boost::weak_ptr<Processor>)> method) {
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

	boost::shared_ptr<Processor> nth_plugin (uint32_t n);
	boost::shared_ptr<Processor> nth_send (uint32_t n);

	bool processor_is_prefader (boost::shared_ptr<Processor> p);

        bool has_io_processor_named (const std::string&);
	ChanCount max_processor_streams () const { return processor_max_streams; }

	/* special processors */

	boost::shared_ptr<Delivery>         monitor_send() const { return _monitor_send; }
	boost::shared_ptr<Delivery>         main_outs() const { return _main_outs; }
	boost::shared_ptr<InternalReturn>   internal_return() const { return _intreturn; }
	boost::shared_ptr<MonitorProcessor> monitor_control() const { return _monitor_control; }
	boost::shared_ptr<Send>             internal_send_for (boost::shared_ptr<const Route> target) const;
	void add_internal_return ();
	BufferSet* get_return_buffer () const;
	void release_return_buffer () const;
	void put_monitor_send_at (Placement);

	/** A record of the stream configuration at some point in the processor list.
	 * Used to return where and why an processor list configuration request failed.
	 */
	struct ProcessorStreams {
		ProcessorStreams(size_t i=0, ChanCount c=ChanCount()) : index(i), count(c) {}

		uint32_t  index; ///< Index of processor where configuration failed
		ChanCount count; ///< Input requested of processor
	};

	int add_processor (boost::shared_ptr<Processor>, Placement placement, ProcessorStreams* err = 0);
	int add_processor (boost::shared_ptr<Processor>, ProcessorList::iterator iter, ProcessorStreams* err = 0, bool activation_allowed = true);
	int add_processors (const ProcessorList&, boost::shared_ptr<Processor> before, ProcessorStreams* err = 0);
	int add_processors (const ProcessorList&, ProcessorList::iterator iter, ProcessorStreams* err = 0);
	int remove_processor (boost::shared_ptr<Processor>, ProcessorStreams* err = 0);
	int remove_processors (const ProcessorList&, ProcessorStreams* err = 0);
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

	PBD::Signal0<void>       active_changed;
	PBD::Signal0<void>       phase_invert_changed;
	PBD::Signal0<void>       denormal_protection_changed;
	PBD::Signal1<void,void*> listen_changed;
	PBD::Signal2<void,bool,void*> solo_changed;
	PBD::Signal1<void,void*> solo_safe_changed;
	PBD::Signal1<void,void*> solo_isolated_changed;
	PBD::Signal1<void,void*> comment_changed;
	PBD::Signal1<void,void*> mute_changed;
	PBD::Signal0<void>       mute_points_changed;

	/** the processors have changed; the parameter indicates what changed */
	PBD::Signal1<void,RouteProcessorChange> processors_changed;
	PBD::Signal1<void,void*> record_enable_changed;
	/** the metering point has changed */
	PBD::Signal0<void>       meter_change; 
	PBD::Signal0<void>       signal_latency_changed;
	PBD::Signal0<void>       initial_delay_changed;
	PBD::Signal0<void>       order_key_changed;
	PBD::Signal0<void>       io_changed;

	/* gui's call this for their own purposes. */

	PBD::Signal2<void,std::string,void*> gui_changed;

	/* stateful */

	XMLNode& get_state();
	int set_state (const XMLNode&, int version);
	virtual XMLNode& get_template();

	XMLNode& get_processor_state ();
	virtual void set_processor_state (const XMLNode&);

	int save_as_template (const std::string& path, const std::string& name);

	PBD::Signal1<void,void*> SelectedChanged;

	int listen_via (boost::shared_ptr<Route>, Placement p, bool active, bool aux);
	void drop_listen (boost::shared_ptr<Route>);

       /** 
        * return true if this route feeds the first argument via at least one
        * (arbitrarily long) signal pathway.
        */
        bool feeds (boost::shared_ptr<Route>, bool* via_send_only = 0);

       /** 
        * return true if this route feeds the first argument directly, via
        * either its main outs or a send.
        */
	bool direct_feeds (boost::shared_ptr<Route>, bool* via_send_only = 0);

        struct FeedRecord {
            boost::weak_ptr<Route> r;
            bool sends_only;

            FeedRecord (boost::shared_ptr<Route> rp, bool sendsonly)
                    : r (rp)
                    , sends_only (sendsonly) {}
        };

        struct FeedRecordCompare {
            bool operator() (const FeedRecord& a, const FeedRecord& b) const {
                    return a.r < b.r;
            }
        };

        typedef std::set<FeedRecord,FeedRecordCompare> FedBy;

        const FedBy& fed_by() const { return _fed_by; }
        void clear_fed_by ();
        bool add_fed_by (boost::shared_ptr<Route>, bool sends_only);
        bool not_fed() const { return _fed_by.empty(); }

	/* Controls (not all directly owned by the Route */

	boost::shared_ptr<AutomationControl> get_control (const Evoral::Parameter& param);

	struct SoloControllable : public AutomationControl {
		SoloControllable (std::string name, Route&);
		void set_value (double);
		double get_value (void) const;

		Route& route;
	};

	struct MuteControllable : public AutomationControl {
		MuteControllable (std::string name, Route&);
		void set_value (double);
		double get_value (void) const;

		Route& route;
	};

	boost::shared_ptr<AutomationControl> solo_control() const {
		return _solo_control;
	}

	boost::shared_ptr<AutomationControl> mute_control() const {
		return _mute_control;
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

	void set_remote_control_id (uint32_t id, bool notify_class_listeners = true);
	uint32_t remote_control_id () const;

	/* for things concerned about *this* route's RID */

	PBD::Signal0<void> RemoteControlIDChanged;

	/* for things concerned about any route's RID changes */

	static PBD::Signal0<void> RemoteControlIDChange;

	void sync_order_keys (std::string const &);
	static PBD::Signal1<void,std::string const &> SyncOrderKeys;

  protected:
	friend class Session;

	void catch_up_on_solo_mute_override ();
	void mod_solo_by_others_upstream (int32_t);
	void mod_solo_by_others_downstream (int32_t);
	bool has_external_redirects() const;
	void curve_reallocate ();
	void just_meter_input (sframes_t start_frame, sframes_t end_frame, nframes_t nframes);
	virtual void set_block_size (nframes_t nframes);

  protected:
	nframes_t check_initial_delay (nframes_t, nframes_t&);

	void passthru (sframes_t start_frame, sframes_t end_frame,
			nframes_t nframes, int declick);

	virtual void write_out_of_band_data (BufferSet& /* bufs */, sframes_t /* start_frame */, sframes_t /* end_frame */,
			nframes_t /* nframes */) {}

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
	boost::shared_ptr<Delivery> _monitor_send;
	boost::shared_ptr<InternalReturn> _intreturn;
	boost::shared_ptr<MonitorProcessor> _monitor_control;

	Flag           _flags;
	int            _pending_declick;
	MeterPoint     _meter_point;
	boost::dynamic_bitset<> _phase_invert;
	bool           _self_solo;
	uint32_t       _soloed_by_others_upstream;
	uint32_t       _soloed_by_others_downstream;
	uint32_t       _solo_isolated;

	bool           _denormal_protection;

	bool _recordable : 1;
	bool _silent : 1;
	bool _declickable : 1;

	boost::shared_ptr<SoloControllable> _solo_control;
	boost::shared_ptr<MuteControllable> _mute_control;
	boost::shared_ptr<MuteMaster> _mute_master;
    
	std::string    _comment;
	bool           _have_internal_generator;
	bool           _solo_safe;
	DataType       _default_type;
        FedBy          _fed_by;

        virtual ChanCount input_streams () const;

  protected:
	virtual XMLNode& state(bool);

	int configure_processors (ProcessorStreams*);

	void passthru_silence (sframes_t start_frame, sframes_t end_frame,
	                       nframes_t nframes, int declick);

	void silence (nframes_t);
	void silence_unlocked (nframes_t);

	ChanCount processor_max_streams;
	uint32_t _remote_control_id;

	uint32_t pans_required() const;
	ChanCount n_process_buffers ();
	
	virtual int  _set_state (const XMLNode&, int, bool call_base);

	boost::shared_ptr<Amp>       _amp;
	boost::shared_ptr<PeakMeter> _meter;

  private:
	int _set_state_2X (const XMLNode&, int);
	void set_processor_state_2X (XMLNodeList const &, int);

	static uint32_t order_key_cnt;

	typedef std::map<std::string, long> OrderKeys;
	OrderKeys order_keys;

	void input_change_handler (IOChange, void *src);
	void output_change_handler (IOChange, void *src);

	bool _in_configure_processors;

	int configure_processors_unlocked (ProcessorStreams*);

	bool add_processor_from_xml (const XMLNode&, ProcessorList::iterator iter);	
	bool add_processor_from_xml_2X (const XMLNode&, int, ProcessorList::iterator iter);	

	void placement_range (Placement p, ProcessorList::iterator& start, ProcessorList::iterator& end);

	void set_self_solo (bool yn);
	void set_mute_master_solo ();

	void set_processor_positions ();
};

} // namespace ARDOUR

#endif /* __ardour_route_h__ */
