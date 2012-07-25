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
#include <boost/enable_shared_from_this.hpp>

#include <glibmm/threads.h>
#include "pbd/fastlog.h"
#include "pbd/xml++.h"
#include "pbd/undo.h"
#include "pbd/stateful.h"
#include "pbd/controllable.h"
#include "pbd/destructible.h"

#include "ardour/ardour.h"
#include "ardour/instrument_info.h"
#include "ardour/io.h"
#include "ardour/types.h"
#include "ardour/mute_master.h"
#include "ardour/route_group_member.h"
#include "ardour/graphnode.h"
#include "ardour/automatable.h"
#include "ardour/unknown_processor.h"

namespace ARDOUR {

class Amp;
class Delivery;
class IOProcessor;
class Panner;
class PannerShell;
class PortSet;
class Processor;
class RouteGroup;
class Send;
class InternalReturn;
class MonitorProcessor;
class Pannable;
class CapturingProcessor;
class InternalSend;

class Route : public SessionObject, public Automatable, public RouteGroupMember, public GraphNode, public boost::enable_shared_from_this<Route>
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
	void set_active (bool yn, void *);

	static std::string ensure_track_or_route_name(std::string, Session &);

	std::string comment() { return _comment; }
	void set_comment (std::string str, void *src);

	bool set_name (const std::string& str);
	static void set_name_in_state (XMLNode &, const std::string &);

        uint32_t order_key (RouteSortOrderKey) const;
        bool has_order_key (RouteSortOrderKey) const;
	void set_order_key (RouteSortOrderKey, uint32_t);
        void sync_order_keys (RouteSortOrderKey);

	bool is_hidden() const { return _flags & Hidden; }
	bool is_master() const { return _flags & MasterOut; }
	bool is_monitor() const { return _flags & MonitorOut; }

	virtual MonitorState monitoring_state () const;
	virtual MeterState metering_state () const;
	
	/* these are the core of the API of a Route. see the protected sections as well */

	virtual int roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	                  int declick, bool& need_butler);

	virtual int no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	                     bool state_changing);

	virtual int silent_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame,
	                         bool& need_butler);

	virtual bool can_record() { return false; }

	virtual void set_record_enabled (bool /*yn*/, void * /*src*/) {}
	virtual bool record_enabled() const { return false; }
	virtual void nonrealtime_handle_transport_stopped (bool abort, bool did_locate, bool flush_processors);
	virtual void realtime_handle_transport_stopped () {}
	virtual void realtime_locate () {}
        virtual void non_realtime_locate (framepos_t);
	virtual void set_pending_declick (int);

	/* end of vfunc-based API */

	void shift (framepos_t, framecnt_t);

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
	void cancel_solo_after_disconnect (bool upstream);

	bool soloed_by_others () const { return _soloed_by_others_upstream||_soloed_by_others_downstream; }
	bool soloed_by_others_upstream () const { return _soloed_by_others_upstream; }
	bool soloed_by_others_downstream () const { return _soloed_by_others_downstream; }
	bool self_soloed () const { return _self_solo; }

	void set_solo_isolated (bool yn, void *src);
	bool solo_isolated() const;

	void set_solo_safe (bool yn, void *src);
	bool solo_safe() const;

	void set_listen (bool yn, void* src);
	bool listening_via_monitor () const;
	void enable_monitor_send ();

	void set_phase_invert (uint32_t, bool yn);
	void set_phase_invert (boost::dynamic_bitset<>);
	bool phase_invert (uint32_t) const;
	boost::dynamic_bitset<> phase_invert () const;

	void set_denormal_protection (bool yn);
	bool denormal_protection() const;

	void         set_meter_point (MeterPoint, bool force = false);
	MeterPoint   meter_point() const { return _meter_point; }
	void         meter ();

	/* Processors */

	boost::shared_ptr<Amp> amp() const  { return _amp; }
	PeakMeter&       peak_meter()       { return *_meter.get(); }
	const PeakMeter& peak_meter() const { return *_meter.get(); }
	boost::shared_ptr<PeakMeter> shared_peak_meter() const { return _meter; }

	void flush_processors ();

	void foreach_processor (boost::function<void(boost::weak_ptr<Processor>)> method) {
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			if (boost::dynamic_pointer_cast<UnknownProcessor> (*i)) {
				break;
			}
			method (boost::weak_ptr<Processor> (*i));
		}
	}

	boost::shared_ptr<Processor> nth_processor (uint32_t n) {
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		ProcessorList::iterator i;
		for (i = _processors.begin(); i != _processors.end() && n; ++i, --n) {}
		if (i == _processors.end()) {
			return boost::shared_ptr<Processor> ();
		} else {
			return *i;
		}
	}

	boost::shared_ptr<Processor> processor_by_id (PBD::ID) const;

	boost::shared_ptr<Processor> nth_plugin (uint32_t n);
	boost::shared_ptr<Processor> nth_send (uint32_t n);

	bool has_io_processor_named (const std::string&);
	ChanCount max_processor_streams () const { return processor_max_streams; }

	std::list<std::string> unknown_processors () const;

	/* special processors */

	boost::shared_ptr<InternalSend>     monitor_send() const { return _monitor_send; }
	boost::shared_ptr<Delivery>         main_outs() const { return _main_outs; }
	boost::shared_ptr<InternalReturn>   internal_return() const { return _intreturn; }
	boost::shared_ptr<MonitorProcessor> monitor_control() const { return _monitor_control; }
	boost::shared_ptr<Send>             internal_send_for (boost::shared_ptr<const Route> target) const;
	void add_internal_return ();
	void add_send_to_internal_return (InternalSend *);
	void remove_send_from_internal_return (InternalSend *);
	void listen_position_changed ();
	boost::shared_ptr<CapturingProcessor> add_export_point(/* Add some argument for placement later */);

	/** A record of the stream configuration at some point in the processor list.
	 * Used to return where and why an processor list configuration request failed.
	 */
	struct ProcessorStreams {
		ProcessorStreams(size_t i=0, ChanCount c=ChanCount()) : index(i), count(c) {}

		uint32_t  index; ///< Index of processor where configuration failed
		ChanCount count; ///< Input requested of processor
	};

	int add_processor (boost::shared_ptr<Processor>, Placement placement, ProcessorStreams* err = 0, bool activation_allowed = true);
	int add_processor_by_index (boost::shared_ptr<Processor>, int, ProcessorStreams* err = 0, bool activation_allowed = true);
	int add_processor (boost::shared_ptr<Processor>, boost::shared_ptr<Processor>, ProcessorStreams* err = 0, bool activation_allowed = true);
	int add_processors (const ProcessorList&, boost::shared_ptr<Processor>, ProcessorStreams* err = 0);
	boost::shared_ptr<Processor> before_processor_for_placement (Placement);
	boost::shared_ptr<Processor> before_processor_for_index (int);
	int remove_processor (boost::shared_ptr<Processor>, ProcessorStreams* err = 0, bool need_process_lock = true);
	int remove_processors (const ProcessorList&, ProcessorStreams* err = 0);
	int reorder_processors (const ProcessorList& new_order, ProcessorStreams* err = 0);
	void disable_processors (Placement);
	void disable_processors ();
	void disable_plugins (Placement);
	void disable_plugins ();
	void ab_plugins (bool forward);
	void clear_processors (Placement);
	void all_visible_processors_active (bool);

	framecnt_t set_private_port_latencies (bool playback) const;
	void       set_public_port_latencies (framecnt_t, bool playback) const;

	framecnt_t   update_signal_latency();
	virtual void set_latency_compensation (framecnt_t);

	void set_user_latency (framecnt_t);
	framecnt_t initial_delay() const { return _initial_delay; }
	framecnt_t signal_latency() const { return _signal_latency; }

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

	/** Emitted with the process lock held */
	PBD::Signal0<void>       io_changed;

	/* gui's call this for their own purposes. */

	PBD::Signal2<void,std::string,void*> gui_changed;

	/* stateful */

	XMLNode& get_state();
	virtual int set_state (const XMLNode&, int version);
	virtual XMLNode& get_template();

	XMLNode& get_processor_state ();
	virtual void set_processor_state (const XMLNode&);

	int save_as_template (const std::string& path, const std::string& name);

	PBD::Signal1<void,void*> SelectedChanged;

	int add_aux_send (boost::shared_ptr<Route>, boost::shared_ptr<Processor>);
	void remove_aux_or_listen (boost::shared_ptr<Route>);

	/**
	 * return true if this route feeds the first argument via at least one
	 * (arbitrarily long) signal pathway.
	 */
	bool feeds (boost::shared_ptr<Route>, bool* via_send_only = 0);

	/**
	 * return true if this route feeds the first argument directly, via
	 * either its main outs or a send.  This is checked by the actual
	 * connections, rather than by what the graph is currently doing.
	 */
	bool direct_feeds_according_to_reality (boost::shared_ptr<Route>, bool* via_send_only = 0);

	/**
	 * return true if this route feeds the first argument directly, via
	 * either its main outs or a send, according to the graph that
	 * is currently being processed.
	 */
	bool direct_feeds_according_to_graph (boost::shared_ptr<Route>, bool* via_send_only = 0);

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

	/* Controls (not all directly owned by the Route */

	boost::shared_ptr<AutomationControl> get_control (const Evoral::Parameter& param);

	class SoloControllable : public AutomationControl {
	public:
		SoloControllable (std::string name, boost::shared_ptr<Route>);
		void set_value (double);
		double get_value () const;

	private:
		boost::weak_ptr<Route> _route;
	};

	struct MuteControllable : public AutomationControl {
	public:
		MuteControllable (std::string name, boost::shared_ptr<Route>);
		void set_value (double);
		double get_value () const;

	private:
		boost::weak_ptr<Route> _route;
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

	boost::shared_ptr<Panner> panner() const;  /* may return null */
	boost::shared_ptr<PannerShell> panner_shell() const;
	boost::shared_ptr<AutomationControl> gain_control() const;
	boost::shared_ptr<Pannable> pannable() const;

	/**
	   Return the first processor that accepts has at least one MIDI input
	   and at least one audio output. In the vast majority of cases, this
	   will be "the instrument". This does not preclude other MIDI->audio
	   processors later in the processing chain, but that would be a
	   special case not covered by this utility function.
	*/
	boost::shared_ptr<Processor> the_instrument() const;
        InstrumentInfo& instrument_info() { return _instrument_info; }

	void protect_automation ();

	enum { 
		/* These numbers are taken from MIDI Machine Control,
		   which can only control up to 317 tracks without
		   doing sysex segmentation.
		*/
		MasterBusRemoteControlID = 318,
		MonitorBusRemoteControlID = 319,
	};

	void     set_remote_control_id (uint32_t id, bool notify_class_listeners = true);
	uint32_t remote_control_id () const;
        void     set_remote_control_id_from_order_key (RouteSortOrderKey, uint32_t order_key);

	/* for things concerned about *this* route's RID */

	PBD::Signal0<void> RemoteControlIDChanged;

	/* for things concerned about *any* route's RID changes */

	static PBD::Signal0<void> RemoteControlIDChange;
	static PBD::Signal1<void,RouteSortOrderKey> SyncOrderKeys;

	bool has_external_redirects() const;

  protected:
	friend class Session;

	void catch_up_on_solo_mute_override ();
	void mod_solo_by_others_upstream (int32_t);
	void mod_solo_by_others_downstream (int32_t);
	void curve_reallocate ();
	virtual void set_block_size (pframes_t nframes);

  protected:
	virtual framecnt_t check_initial_delay (framecnt_t nframes, framepos_t&) { return nframes; }

	void passthru (framepos_t start_frame, framepos_t end_frame,
			pframes_t nframes, int declick);

	virtual void write_out_of_band_data (BufferSet& /* bufs */, framepos_t /* start_frame */, framepos_t /* end_frame */,
			framecnt_t /* nframes */) {}

	virtual void process_output_buffers (BufferSet& bufs,
	                                     framepos_t start_frame, framepos_t end_frame,
	                                     pframes_t nframes, int declick,
	                                     bool gain_automation_ok);

	boost::shared_ptr<IO> _input;
	boost::shared_ptr<IO> _output;

	bool           _active;
	framecnt_t     _signal_latency;
	framecnt_t     _initial_delay;
	framecnt_t     _roll_delay;

	ProcessorList  _processors;
	mutable Glib::Threads::RWLock   _processor_lock;
	boost::shared_ptr<Delivery> _main_outs;
	boost::shared_ptr<InternalSend> _monitor_send;
	boost::shared_ptr<InternalReturn> _intreturn;
	boost::shared_ptr<MonitorProcessor> _monitor_control;
	boost::shared_ptr<Pannable> _pannable;

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

	virtual void act_on_mute () {}

	std::string    _comment;
	bool           _have_internal_generator;
	bool           _solo_safe;
	DataType       _default_type;
	FedBy          _fed_by;

        InstrumentInfo _instrument_info;

	virtual ChanCount input_streams () const;

  protected:
	virtual XMLNode& state(bool);

	int configure_processors (ProcessorStreams*);

	void passthru_silence (framepos_t start_frame, framepos_t end_frame,
	                       pframes_t nframes, int declick);

	void silence (framecnt_t);
	void silence_unlocked (framecnt_t);

	ChanCount processor_max_streams;

	uint32_t pans_required() const;
	ChanCount n_process_buffers ();

	virtual void maybe_declick (BufferSet&, framecnt_t, int);

	boost::shared_ptr<Amp>       _amp;
	boost::shared_ptr<PeakMeter> _meter;

  private:
	int set_state_2X (const XMLNode&, int);
	void set_processor_state_2X (XMLNodeList const &, int);

	typedef std::map<RouteSortOrderKey,uint32_t> OrderKeys;
 	OrderKeys order_keys;
        uint32_t _remote_control_id;

	void input_change_handler (IOChange, void *src);
	void output_change_handler (IOChange, void *src);

	bool input_port_count_changing (ChanCount);

	bool _in_configure_processors;

	int configure_processors_unlocked (ProcessorStreams*);
	std::list<std::pair<ChanCount, ChanCount> > try_configure_processors (ChanCount, ProcessorStreams *);
	std::list<std::pair<ChanCount, ChanCount> > try_configure_processors_unlocked (ChanCount, ProcessorStreams *);

	bool add_processor_from_xml_2X (const XMLNode&, int);

	void placement_range (Placement p, ProcessorList::iterator& start, ProcessorList::iterator& end);

	void set_self_solo (bool yn);
	void set_mute_master_solo ();

	void set_processor_positions ();
	framecnt_t update_port_latencies (PortSet& ports, PortSet& feeders, bool playback, framecnt_t) const;

	void setup_invisible_processors ();
	void unpan ();

	boost::shared_ptr<CapturingProcessor> _capturing_processor;

	/** A handy class to keep processor state while we attempt a reconfiguration
	 *  that may fail.
	 */
	class ProcessorState {
	public:
		ProcessorState (Route* r)
			: _route (r)
			, _processors (r->_processors)
			, _processor_max_streams (r->processor_max_streams)
		{ }

		void restore () {
			_route->_processors = _processors;
			_route->processor_max_streams = _processor_max_streams;
		}

	private:
		/* this should perhaps be a shared_ptr, but ProcessorStates will
		   not hang around long enough for it to matter.
		*/
		Route* _route;
		ProcessorList _processors;
		ChanCount _processor_max_streams;
	};

	friend class ProcessorState;

	/* no copy construction */
	Route (Route const &);

	void maybe_note_meter_position ();
	
	/** true if we've made a note of a custom meter position in these variables */
	bool _custom_meter_position_noted;
	/** the processor that came after the meter when it was last set to a custom position,
	    or 0.
	*/
	boost::weak_ptr<Processor> _processor_after_last_custom_meter;
	/** true if the last custom meter position was at the end of the processor list */
	bool _last_custom_meter_was_at_end;

        void reset_instrument_info ();
};

} // namespace ARDOUR

#endif /* __ardour_route_h__ */
