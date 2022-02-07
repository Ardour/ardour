/*
 * Copyright (C) 2000-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2018 Len Ovens <len@ovenwerks.net>
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

#include <glibmm/threads.h>
#include "pbd/fastlog.h"
#include "pbd/xml++.h"
#include "pbd/undo.h"
#include "pbd/stateful.h"
#include "pbd/controllable.h"
#include "pbd/destructible.h"
#include "pbd/g_atomic_compat.h"

#include "ardour/ardour.h"
#include "ardour/gain_control.h"
#include "ardour/instrument_info.h"
#include "ardour/io.h"
#include "ardour/io_vector.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/monitorable.h"
#include "ardour/muteable.h"
#include "ardour/mute_master.h"
#include "ardour/mute_control.h"
#include "ardour/route_group_member.h"
#include "ardour/stripable.h"
#include "ardour/graphnode.h"
#include "ardour/automatable.h"
#include "ardour/unknown_processor.h"
#include "ardour/soloable.h"
#include "ardour/solo_control.h"
#include "ardour/solo_safe_control.h"
#include "ardour/slavable.h"

class RoutePinWindowProxy;
class PatchChangeGridDialog;

namespace ARDOUR {

class Amp;
class BeatBox;
class DelayLine;
class Delivery;
class DiskReader;
class DiskWriter;
class IOProcessor;
class Panner;
class PannerShell;
class PolarityProcessor;
class PortSet;
class Processor;
class PluginInsert;
class RouteGroup;
class Send;
class InternalReturn;
class Location;
class MonitorControl;
class MonitorProcessor;
class Pannable;
class CapturingProcessor;
class InternalSend;
class VCA;
class SoloIsolateControl;
class PhaseControl;
class MonitorControl;
class TriggerBox;

class LIBARDOUR_API Route : public Stripable,
                            public GraphNode,
                            public Soloable,
                            public Muteable,
                            public Monitorable,
                            public RouteGroupMember
{
public:

	typedef std::list<boost::shared_ptr<Processor> > ProcessorList;

	Route (Session&, std::string name, PresentationInfo::Flag flags = PresentationInfo::Flag(0), DataType default_type = DataType::AUDIO);
	virtual ~Route();

	virtual int init ();

	DataType data_type () const {
		/* XXX ultimately nice to do away with this concept, but it is
		   quite useful for coders and for users too.
		*/
		return _default_type;
	}

	boost::shared_ptr<IO> input() const { return _input; }
	boost::shared_ptr<IO> output() const { return _output; }
	IOVector all_inputs () const;
	IOVector all_outputs () const;

	ChanCount n_inputs() const { return _input->n_ports(); }
	ChanCount n_outputs() const { return _output->n_ports(); }

	bool active() const { return _active; }
	void set_active (bool yn, void *);

	std::string ensure_track_or_route_name (std::string) const;

	std::string comment() { return _comment; }
	void set_comment (std::string str, void *src);

	bool set_name (const std::string& str);
	static void set_name_in_state (XMLNode &, const std::string &);

	boost::shared_ptr<MonitorControl> monitoring_control() const { return _monitoring_control; }

	MonitorState monitoring_state () const;
	virtual MonitorState get_input_monitoring_state (bool recording, bool talkback) const { return MonitoringSilence; }

	/* these are the core of the API of a Route. see the protected sections as well */

	virtual void filter_input (BufferSet &) {}

	int roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler);

	int no_roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool state_changing);

	int silent_roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler);

	virtual bool declick_in_progress () const { return false; }
	virtual bool can_record() { return false; }

	void non_realtime_transport_stop (samplepos_t now, bool flush);
	virtual void realtime_handle_transport_stopped ();

	virtual void realtime_locate (bool) {}
	virtual void non_realtime_locate (samplepos_t);
	void set_loop (ARDOUR::Location *);

	/* end of vfunc-based API */

	void shift (timepos_t const &, timecnt_t const &);

	/* controls use set_solo() to modify this route's solo state */

	void clear_all_solo_state ();

	bool soloed_by_others () const { return _solo_control->soloed_by_others(); }
	bool soloed_by_others_upstream () const { return _solo_control->soloed_by_others_upstream(); }
	bool soloed_by_others_downstream () const { return _solo_control->soloed_by_others_downstream(); }
	bool self_soloed () const { return _solo_control->self_soloed(); }
	bool soloed () const { return self_soloed () || soloed_by_others (); }

	void push_solo_upstream (int32_t delta);
	void push_solo_isolate_upstream (int32_t delta);
	bool can_solo () const {
		return !(is_master() || is_monitor() || is_auditioner() || is_foldbackbus());
	}
	bool is_safe () const {
		return _solo_safe_control->get_value();
	}
	bool can_monitor () const {
		return can_solo() || is_foldbackbus ();
	}
	void enable_monitor_send ();

	void set_denormal_protection (bool yn);
	bool denormal_protection() const;

	void         set_meter_point (MeterPoint);
	bool         apply_processor_changes_rt ();
	void         emit_pending_signals ();
	MeterPoint   meter_point() const { return _pending_meter_point; }

	void update_send_delaylines ();

	void         set_meter_type (MeterType t);
	MeterType    meter_type () const;

	void set_disk_io_point (DiskIOPoint);
	DiskIOPoint disk_io_point() const { return _disk_io_point; }

	void stop_triggers (bool now);

	/* Processors */

	boost::shared_ptr<Amp> amp() const  { return _amp; }
	boost::shared_ptr<Amp> trim() const { return _trim; }
	boost::shared_ptr<PolarityProcessor> polarity() const { return _polarity; }
	boost::shared_ptr<PeakMeter>       peak_meter()       { return _meter; }
	boost::shared_ptr<const PeakMeter> peak_meter() const { return _meter; }
	boost::shared_ptr<PeakMeter> shared_peak_meter() const { return _meter; }
	boost::shared_ptr<TriggerBox> triggerbox() const { return _triggerbox; }

	void flush_processors ();

	void foreach_processor (boost::function<void(boost::weak_ptr<Processor>)> method) {
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
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

	boost::shared_ptr<Processor> nth_plugin (uint32_t n) const;
	boost::shared_ptr<Processor> nth_send (uint32_t n) const;

	bool has_io_processor_named (const std::string&);
	ChanCount max_processor_streams () const { return processor_max_streams; }

	std::list<std::string> unknown_processors () const;

	RoutePinWindowProxy * pinmgr_proxy () const { return _pinmgr_proxy; }
	void set_pingmgr_proxy (RoutePinWindowProxy* wp) { _pinmgr_proxy = wp ; }

	PatchChangeGridDialog* patch_selector_dialog () const { return _patch_selector_dialog; }
	void set_patch_selector_dialog  (PatchChangeGridDialog* d) { _patch_selector_dialog = d; }

	boost::shared_ptr<AutomationControl> automation_control_recurse (PBD::ID const & id) const;

	/* special processors */

	boost::shared_ptr<InternalSend>     monitor_send() const { return _monitor_send; }
	/** the signal processorat at end of the processing chain which produces output */
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
	bool processors_reorder_needs_configure (const ProcessorList& new_order);
	/** remove plugin/processor
	 *
	 * @param proc processor to remove
	 * @param err error report (index where removal vailed, channel-count why it failed) may be nil
	 * @param need_process_lock if locking is required (set to true, unless called from RT context with lock)
	 * @returns 0 on success
	 */
	int remove_processor (boost::shared_ptr<Processor> proc, ProcessorStreams* err = 0, bool need_process_lock = true);
	/** replace plugin/processor with another
	 *
	 * @param old processor to remove
	 * @param sub processor to substitute the old one with
	 * @param err error report (index where removal vailed, channel-count why it failed) may be nil
	 * @returns 0 on success
	 */
	int replace_processor (boost::shared_ptr<Processor> old, boost::shared_ptr<Processor> sub, ProcessorStreams* err = 0);
	int remove_processors (const ProcessorList&, ProcessorStreams* err = 0);
	int reorder_processors (const ProcessorList& new_order, ProcessorStreams* err = 0);
	void disable_processors (Placement);
	void disable_processors ();
	void disable_plugins (Placement);
	void disable_plugins ();
	void ab_plugins (bool forward);
	void clear_processors (Placement);
	void all_visible_processors_active (bool);
	void move_instrument_down (bool postfader = false);

	bool strict_io () const { return _strict_io; }
	bool set_strict_io (bool);
	/** reset plugin-insert configuration to default, disable customizations.
	 *
	 * This is equivalent to calling
	 * @code
	 * customize_plugin_insert (proc, 0, unused)
	 * @endcode
	 *
	 * @param proc Processor to reset
	 * @returns true if successful
	 */
	bool reset_plugin_insert (boost::shared_ptr<Processor> proc);
	/** enable custom plugin-insert configuration
	 * @param proc Processor to customize
	 * @param count number of plugin instances to use (if zero, reset to default)
	 * @param outs output port customization
	 * @param sinks input pins for variable-I/O plugins
	 * @returns true if successful
	 */
	bool customize_plugin_insert (boost::shared_ptr<Processor> proc, uint32_t count, ChanCount outs, ChanCount sinks);
	bool add_remove_sidechain (boost::shared_ptr<Processor> proc, bool);
	bool plugin_preset_output (boost::shared_ptr<Processor> proc, ChanCount outs);

	/* enable sidechain input for a given processor
	 *
	 * The sidechain itself is an IO port object with variable number of channels and configured independently.
	 * Adding/removing the port itself however requires reconfiguring the route and is hence
	 * not a plugin operation itself.
	 *
	 * @param proc the processor to add sidechain inputs to
	 * @returns true on success
	 */
	bool add_sidechain (boost::shared_ptr<Processor> proc) { return add_remove_sidechain (proc, true); }
	/* remove sidechain input from given processor
	 * @param proc the processor to remove the sidechain input from
	 * @returns true on success
	 */
	bool remove_sidechain (boost::shared_ptr<Processor> proc) { return add_remove_sidechain (proc, false); }

	samplecnt_t  update_signal_latency (bool apply_to_delayline = false, bool* delayline_update_needed = NULL);
	void apply_latency_compensation ();

	samplecnt_t  set_private_port_latencies (bool playback) const;
	void         set_public_port_latencies (samplecnt_t, bool playback, bool with_latcomp) const;

	samplecnt_t signal_latency() const { return _signal_latency; }
	samplecnt_t playback_latency (bool incl_downstream = false) const;

	virtual samplecnt_t output_latency () const { return _output_latency; }

	PBD::Signal0<void> active_changed;
	PBD::Signal0<void> denormal_protection_changed;
	PBD::Signal0<void> comment_changed;

	bool is_track();

	/** track numbers - assigned by session
	 * nubers > 0 indicate tracks (audio+midi)
	 * nubers < 0 indicate busses
	 * zero is reserved for unnumbered special busses.
	 * */
	PBD::Signal0<void> track_number_changed;
	int64_t track_number() const { return _track_number; }

	void set_track_number(int64_t tn) {
		if (tn == _track_number) { return; }
		_track_number = tn;
		track_number_changed();
		PropertyChanged (ARDOUR::Properties::name);
	}

	enum PluginSetupOptions {
		None = 0x0,
		CanReplace = 0x1,
		MultiOut = 0x2,
	};

	/** ask GUI about port-count, fan-out when adding instrument */
	static PBD::Signal3<int, boost::shared_ptr<Route>, boost::shared_ptr<PluginInsert>, PluginSetupOptions > PluginSetup;

	/** used to signal the GUI to fan-out (track-creation) */
	static PBD::Signal1<void, boost::weak_ptr<Route> > FanOut;

	/** the processors have changed; the parameter indicates what changed */
	PBD::Signal1<void,RouteProcessorChange> processors_changed;
	PBD::Signal1<void,void*> record_enable_changed;
	/** a processor's latency has changed
	 * (emitted from PluginInsert::latency_changed)
	 */
	PBD::Signal0<void> processor_latency_changed;
	/** the metering point has changed */
	PBD::Signal0<void> meter_change;

	/** Emitted with the process lock held */
	PBD::Signal0<void>       io_changed;

	/* stateful */
	XMLNode& get_state();
	XMLNode& get_template();
	virtual int set_state (const XMLNode&, int version);

	XMLNode& get_processor_state ();
	void set_processor_state (const XMLNode&, int version);
	virtual bool set_processor_state (XMLNode const & node, int version, XMLProperty const* prop, ProcessorList& new_order, bool& must_configure);

	boost::weak_ptr<Route> weakroute ();

	int save_as_template (const std::string& path, const std::string& name, const std::string& description );

	PBD::Signal1<void,void*> SelectedChanged;

	int add_aux_send (boost::shared_ptr<Route>, boost::shared_ptr<Processor>);
	int add_foldback_send (boost::shared_ptr<Route>, bool post_fader);
	void remove_monitor_send ();

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

	bool feeds_according_to_graph (boost::shared_ptr<Route>);

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

	/* Controls (not all directly owned by the Route) */

	boost::shared_ptr<AutomationControl> get_control (const Evoral::Parameter& param);

	boost::shared_ptr<SoloControl> solo_control() const {
		return _solo_control;
	}

	boost::shared_ptr<MuteControl> mute_control() const {
		return _mute_control;
	}

	bool can_be_muted_by_others () const { return can_solo(); }
	bool muted () const { return _mute_control->muted(); }
	bool muted_by_masters () const { return _mute_control->muted_by_masters(); }
	bool muted_by_self () const { return _mute_control->muted_by_self(); }
	bool muted_by_others_soloing () const;

	boost::shared_ptr<SoloIsolateControl> solo_isolate_control() const {
		return _solo_isolate_control;
	}

	boost::shared_ptr<SoloSafeControl> solo_safe_control() const {
		return _solo_safe_control;
	}

	/* Route doesn't own these items, but sub-objects that it does own have them
	   and to make UI code a bit simpler, we provide direct access to them
	   here.
	*/

	boost::shared_ptr<Panner> panner() const;  /* may return null */
	boost::shared_ptr<PannerShell> panner_shell() const;
	boost::shared_ptr<Pannable> pannable() const;

	boost::shared_ptr<GainControl> gain_control() const;
	boost::shared_ptr<GainControl> trim_control() const;
	boost::shared_ptr<GainControl> volume_control() const;
	boost::shared_ptr<PhaseControl> phase_control() const;

	void set_volume_applies_to_output (bool);

	bool volume_applies_to_output () const {
		return _volume_applies_to_output;
	}

	/**
	   Return the first processor that accepts has at least one MIDI input
	   and at least one audio output. In the vast majority of cases, this
	   will be "the instrument". This does not preclude other MIDI->audio
	   processors later in the processing chain, but that would be a
	   special case not covered by this utility function.
	*/
	boost::shared_ptr<Processor> the_instrument() const;
	InstrumentInfo& instrument_info() { return _instrument_info; }
	bool instrument_fanned_out () const { return _instrument_fanned_out;}


	/* "well-known" controls.
	 * Any or all of these may return NULL.
	 */

	boost::shared_ptr<AutomationControl> pan_azimuth_control() const;
	boost::shared_ptr<AutomationControl> pan_elevation_control() const;
	boost::shared_ptr<AutomationControl> pan_width_control() const;
	boost::shared_ptr<AutomationControl> pan_frontback_control() const;
	boost::shared_ptr<AutomationControl> pan_lfe_control() const;

	uint32_t eq_band_cnt () const;
	std::string eq_band_name (uint32_t) const;
	boost::shared_ptr<AutomationControl> eq_enable_controllable () const;
	boost::shared_ptr<AutomationControl> eq_gain_controllable (uint32_t band) const;
	boost::shared_ptr<AutomationControl> eq_freq_controllable (uint32_t band) const;
	boost::shared_ptr<AutomationControl> eq_q_controllable (uint32_t band) const;
	boost::shared_ptr<AutomationControl> eq_shape_controllable (uint32_t band) const;

	boost::shared_ptr<AutomationControl> filter_freq_controllable (bool hpf) const;
	boost::shared_ptr<AutomationControl> filter_slope_controllable (bool) const;
	boost::shared_ptr<AutomationControl> filter_enable_controllable (bool) const;

	boost::shared_ptr<AutomationControl> tape_drive_controllable () const;
	boost::shared_ptr<ReadOnlyControl>   tape_drive_mtr_controllable () const;

	boost::shared_ptr<AutomationControl> comp_enable_controllable () const;
	boost::shared_ptr<AutomationControl> comp_threshold_controllable () const;
	boost::shared_ptr<AutomationControl> comp_speed_controllable () const;
	boost::shared_ptr<AutomationControl> comp_mode_controllable () const;
	boost::shared_ptr<AutomationControl> comp_makeup_controllable () const;
	boost::shared_ptr<ReadOnlyControl>   comp_redux_controllable () const;

	std::string comp_mode_name (uint32_t mode) const;
	std::string comp_speed_name (uint32_t mode) const;

	boost::shared_ptr<AutomationControl> send_level_controllable (uint32_t n) const;
	boost::shared_ptr<AutomationControl> send_enable_controllable (uint32_t n) const;
	boost::shared_ptr<AutomationControl> send_pan_azimuth_controllable (uint32_t n) const;
	boost::shared_ptr<AutomationControl> send_pan_azimuth_enable_controllable (uint32_t n) const;

	std::string send_name (uint32_t n) const;

	boost::shared_ptr<AutomationControl> master_send_enable_controllable () const;

	boost::shared_ptr<ReadOnlyControl> master_correlation_mtr_controllable (bool) const;

	boost::shared_ptr<AutomationControl> master_limiter_enable_controllable () const;
	boost::shared_ptr<ReadOnlyControl> master_limiter_mtr_controllable () const;
	boost::shared_ptr<ReadOnlyControl> master_k_mtr_controllable () const;

	void protect_automation ();

	bool has_external_redirects() const;

	/* can only be executed by a route for which is_monitor() is true
	 * (i.e. the monitor out)
	 */
	void monitor_run (samplepos_t start_sample, samplepos_t end_sample, pframes_t nframes);

	bool slaved_to (boost::shared_ptr<VCA>) const;
	bool slaved () const;

	virtual void use_captured_sources (SourceList& srcs, CaptureInfos const &) {}

protected:
	friend class Session;

	void catch_up_on_solo_mute_override ();
	void set_listen (bool);

	virtual void set_block_size (pframes_t nframes);

	virtual int no_roll_unlocked (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool session_state_changing);

	virtual void snapshot_out_of_band_data (samplecnt_t /* nframes */) {}
	virtual void write_out_of_band_data (BufferSet&, samplecnt_t /* nframes */) const {}
	virtual void update_controls (BufferSet const&) {}

	void process_output_buffers (BufferSet& bufs,
	                             samplepos_t start_sample, samplepos_t end_sample,
	                             pframes_t nframes,
	                             bool gain_automation_ok,
	                             bool run_disk_processors);

	void flush_processor_buffers_locked (samplecnt_t nframes);

	virtual void bounce_process (BufferSet& bufs,
	                             samplepos_t start_sample, samplecnt_t nframes,
															 boost::shared_ptr<Processor> endpoint, bool include_endpoint,
	                             bool for_export, bool for_freeze);

	samplecnt_t  bounce_get_latency (boost::shared_ptr<Processor> endpoint, bool include_endpoint, bool for_export, bool for_freeze) const;
	ChanCount    bounce_get_output_streams (ChanCount &cc, boost::shared_ptr<Processor> endpoint, bool include_endpoint, bool for_export, bool for_freeze) const;

	bool can_freeze_processor (boost::shared_ptr<Processor>, bool allow_routing = false) const;

	bool           _active;
	samplecnt_t    _signal_latency;
	samplecnt_t    _output_latency;

	ProcessorList  _processors;
	mutable Glib::Threads::RWLock _processor_lock;

	boost::shared_ptr<IO>               _input;
	boost::shared_ptr<IO>               _output;

	boost::shared_ptr<Delivery>         _main_outs;
	boost::shared_ptr<InternalSend>     _monitor_send;
	boost::shared_ptr<InternalReturn>   _intreturn;
	boost::shared_ptr<MonitorProcessor> _monitor_control;
	boost::shared_ptr<Pannable>         _pannable;
	boost::shared_ptr<DiskReader>       _disk_reader;
	boost::shared_ptr<DiskWriter>       _disk_writer;
#ifdef HAVE_BEATBOX
	boost::shared_ptr<BeatBox>       _beatbox;
#endif
	boost::shared_ptr<MonitorControl>   _monitoring_control;

	DiskIOPoint _disk_io_point;

	enum {
		EmitNone = 0x00,
		EmitMeterChanged = 0x01,
		EmitMeterVisibilityChange = 0x02,
		EmitRtProcessorChange = 0x04
	};

	ProcessorList     _pending_processor_order;
	GATOMIC_QUAL gint _pending_process_reorder; // atomic
	GATOMIC_QUAL gint _pending_listen_change; // atomic
	GATOMIC_QUAL gint _pending_signals; // atomic

	MeterPoint     _meter_point;
	MeterPoint     _pending_meter_point;

	bool           _denormal_protection;

	bool _recordable : 1;

	boost::shared_ptr<SoloControl> _solo_control;
	boost::shared_ptr<MuteControl> _mute_control;
	boost::shared_ptr<SoloIsolateControl> _solo_isolate_control;
	boost::shared_ptr<SoloSafeControl> _solo_safe_control;

	std::string    _comment;
	bool           _have_internal_generator;
	DataType       _default_type;
	FedBy          _fed_by;

	InstrumentInfo _instrument_info;
	bool           _instrument_fanned_out;
	Location*      _loop_location;

	virtual ChanCount input_streams () const;

	virtual XMLNode& state (bool save_template);

	int configure_processors (ProcessorStreams*);

	void silence (samplecnt_t);
	void silence_unlocked (samplecnt_t);

	ChanCount processor_max_streams;
	ChanCount processor_out_streams;

	uint32_t pans_required() const;
	ChanCount n_process_buffers ();

	boost::shared_ptr<GainControl>  _gain_control;
	boost::shared_ptr<GainControl>  _trim_control;
	boost::shared_ptr<GainControl>  _volume_control;
	boost::shared_ptr<PhaseControl> _phase_control;
	boost::shared_ptr<Amp>               _amp;
	boost::shared_ptr<Amp>               _trim;
	boost::shared_ptr<Amp>               _volume;
	boost::shared_ptr<PeakMeter>         _meter;
	boost::shared_ptr<PolarityProcessor> _polarity;
	boost::shared_ptr<TriggerBox>        _triggerbox;

	bool _volume_applies_to_output;

	boost::shared_ptr<DelayLine> _delayline;

	bool is_internal_processor (boost::shared_ptr<Processor>) const;

	boost::shared_ptr<Processor> the_instrument_unlocked() const;

	SlavableControlList slavables () const;

private:
	/* no copy construction */
	Route (Route const &);

	int set_state_2X (const XMLNode&, int);
	void set_processor_state_2X (XMLNodeList const &, int);

	void input_change_handler (IOChange, void *src);
	void output_change_handler (IOChange, void *src);
	void sidechain_change_handler (IOChange, void *src);

	void processor_selfdestruct (boost::weak_ptr<Processor>);
	std::vector<boost::weak_ptr<Processor> > selfdestruct_sequence;
	Glib::Threads::Mutex  selfdestruct_lock;

	bool input_port_count_changing (ChanCount);
	bool output_port_count_changing (ChanCount);

	int configure_processors_unlocked (ProcessorStreams*, Glib::Threads::RWLock::WriterLock*);
	bool set_meter_point_unlocked ();
	void apply_processor_order (const ProcessorList& new_order);

	std::list<std::pair<ChanCount, ChanCount> > try_configure_processors (ChanCount, ProcessorStreams *);
	std::list<std::pair<ChanCount, ChanCount> > try_configure_processors_unlocked (ChanCount, ProcessorStreams *);

	bool add_processor_from_xml_2X (const XMLNode&, int);

	void placement_range (Placement p, ProcessorList::iterator& start, ProcessorList::iterator& end);

	void set_self_solo (bool yn);
	void unpan ();

	void set_processor_positions ();
	samplecnt_t update_port_latencies (PortSet& ports, PortSet& feeders, bool playback, samplecnt_t) const;

	void setup_invisible_processors ();

	pframes_t latency_preroll (pframes_t nframes, samplepos_t& start_sample, samplepos_t& end_sample);

	void run_route (samplepos_t start_sample, samplepos_t end_sample, pframes_t nframes, bool gain_automation_ok, bool run_disk_reader);
	void fill_buffers_with_input (BufferSet& bufs, boost::shared_ptr<IO> io, pframes_t nframes);

	void reset_instrument_info ();
	void solo_control_changed (bool self, PBD::Controllable::GroupControlDisposition);
	void maybe_note_meter_position ();

	void set_plugin_state_dir (boost::weak_ptr<Processor>, const std::string&);

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

	boost::shared_ptr<CapturingProcessor> _capturing_processor;

	int64_t _track_number;
	bool    _strict_io;
	bool    _in_configure_processors;
	bool    _initial_io_setup;
	bool    _in_sidechain_setup;
	gain_t  _monitor_gain;

	/** true if we've made a note of a custom meter position in these variables */
	bool _custom_meter_position_noted;
	/** the processor that came after the meter when it was last set to a custom position,
	    or 0.
	*/
	boost::weak_ptr<Processor> _processor_after_last_custom_meter;

	RoutePinWindowProxy*   _pinmgr_proxy;
	PatchChangeGridDialog* _patch_selector_dialog;
};

} // namespace ARDOUR

#endif /* __ardour_route_h__ */
