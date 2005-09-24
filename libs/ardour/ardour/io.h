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

#ifndef __ardour_io_h__
#define __ardour_io_h__

#include <string>
#include <vector>
#include <cmath>
#include <sigc++/signal.h>
#include <jack/jack.h>

#include <pbd/lockmonitor.h>
#include <pbd/fastlog.h>
#include <pbd/undo.h>
#include <pbd/atomic.h>
#include <midi++/controllable.h>

#include <ardour/ardour.h>
#include <ardour/stateful.h>
#include <ardour/utils.h>
#include <ardour/state_manager.h>
#include <ardour/curve.h>

using std::string;
using std::vector;

class XMLNode;

namespace ARDOUR {

class Session;
class AudioEngine;
class Port;
class Connection;
class Panner;

class IO : public Stateful, public ARDOUR::StateManager
{

  public:
	static const string state_node_name;

	IO (Session&, string name, 
	    int input_min = -1, int input_max = -1, 
	    int output_min = -1, int output_max = -1);

	virtual ~IO();

	int input_minimum() const { return _input_minimum; }
	int input_maximum() const { return _input_maximum; }
	int output_minimum() const { return _output_minimum; }
	int output_maximum() const { return _output_maximum; }

	void set_input_minimum (int n);
	void set_input_maximum (int n);
	void set_output_minimum (int n);
	void set_output_maximum (int n);

	const string& name() const { return _name; }
	virtual int set_name (string str, void *src);
	
	virtual void silence  (jack_nframes_t, jack_nframes_t offset);

	void pan (vector<Sample*>& bufs, uint32_t nbufs, jack_nframes_t nframes, jack_nframes_t offset, gain_t gain_coeff);
	void pan_automated (vector<Sample*>& bufs, uint32_t nbufs, jack_nframes_t start_frame, jack_nframes_t end_frame, 
			    jack_nframes_t nframes, jack_nframes_t offset);
	void collect_input  (vector<Sample*>&, uint32_t nbufs, jack_nframes_t nframes, jack_nframes_t offset);
	void deliver_output (vector<Sample *>&, uint32_t nbufs, jack_nframes_t nframes, jack_nframes_t offset);
	void deliver_output_no_pan (vector<Sample *>&, uint32_t nbufs, jack_nframes_t nframes, jack_nframes_t offset);
	void just_meter_input (jack_nframes_t start_frame, jack_nframes_t end_frame, 
			       jack_nframes_t nframes, jack_nframes_t offset);

	virtual uint32_t n_process_buffers () { return 0; }

	virtual void   set_gain (gain_t g, void *src);
	void   inc_gain (gain_t delta, void *src);
	gain_t         gain () const                      { return _desired_gain; }
	virtual gain_t effective_gain () const;

	Panner& panner() { return *_panner; }

	int ensure_io (uint32_t, uint32_t, bool clear, void *src);

	int use_input_connection (Connection&, void *src);
	int use_output_connection (Connection&, void *src);

	Connection *input_connection() const { return _input_connection; }
	Connection *output_connection() const { return _output_connection; }

	int add_input_port (string source, void *src);
	int add_output_port (string destination, void *src);

	int remove_input_port (Port *, void *src);
	int remove_output_port (Port *, void *src);

	int set_input (Port *, void *src);

	int connect_input (Port *our_port, string other_port, void *src);
	int connect_output (Port *our_port, string other_port, void *src);

	int disconnect_input (Port *our_port, string other_port, void *src);
	int disconnect_output (Port *our_port, string other_port, void *src);

	int disconnect_inputs (void *src);
	int disconnect_outputs (void *src);

	jack_nframes_t output_latency() const;
	jack_nframes_t input_latency() const;
	void           set_port_latency (jack_nframes_t);

	Port *output (uint32_t n) const {
		if (n < _noutputs) {
			return _outputs[n];
		} else {
			return 0;
		}
	}

	Port *input (uint32_t n) const {
		if (n < _ninputs) {
			return _inputs[n];
		} else {
			return 0;
		}
	}

	uint32_t n_inputs () const { return _ninputs; }
	uint32_t n_outputs () const { return _noutputs; }

	sigc::signal<void,IOChange,void*> input_changed;
	sigc::signal<void,IOChange,void*> output_changed;

	sigc::signal<void,void*> gain_changed;
	sigc::signal<void,void*> name_changed;

	virtual XMLNode& state (bool full);
	XMLNode& get_state (void);
	int set_state (const XMLNode&);

	virtual UndoAction get_memento() const;


	static int  disable_connecting (void);

	static int  enable_connecting (void);

	static int  disable_ports (void);

	static int  enable_ports (void);

	static int  disable_panners (void);

	static int  reset_panners (void);
	
	static sigc::signal<int> PortsLegal;
	static sigc::signal<int> PannersLegal;
	static sigc::signal<int> ConnectingLegal;
	static sigc::signal<void,uint32_t> MoreOutputs;
	static sigc::signal<int> PortsCreated;

	/* MIDI control */

	void set_midi_to_gain_function (gain_t (*function)(double val)) {
		_midi_gain_control.midi_to_gain = function;
	}

	void set_gain_to_midi_function (double (*function)(gain_t gain)) {
		_midi_gain_control.gain_to_midi = function;
	}

	MIDI::Controllable& midi_gain_control() {
		return _midi_gain_control;
	}

	virtual void reset_midi_control (MIDI::Port*, bool on);

	virtual void send_all_midi_feedback ();
	virtual MIDI::byte* write_midi_feedback (MIDI::byte*, int32_t& bufsize);
	
	/* Peak metering */

	float peak_input_power (uint32_t n) { 
		if (n < std::max(_ninputs, _noutputs)) {
			float x = _stored_peak_power[n];
			if(x > 0.0) {
				return 20 * fast_log10(x);
			} else {
				return minus_infinity();
			}
		} else {
			return minus_infinity();
		}
	}

	static sigc::signal<void> GrabPeakPower;

	/* automation */

	void clear_automation ();

	bool gain_automation_recording() const { 
		return (_gain_automation_curve.automation_state() & (Write|Touch));
	}

	bool gain_automation_playback() const {
		return (_gain_automation_curve.automation_state() & Play) ||
			((_gain_automation_curve.automation_state() & Touch) && 
			 !_gain_automation_curve.touching());
	}

	virtual void set_gain_automation_state (AutoState);
	AutoState gain_automation_state() const { return _gain_automation_curve.automation_state(); }
	sigc::signal<void> gain_automation_state_changed;

	virtual void set_gain_automation_style (AutoStyle);
	AutoStyle gain_automation_style () const { return _gain_automation_curve.automation_style(); }
	sigc::signal<void> gain_automation_style_changed;

	static void set_automation_interval (jack_nframes_t frames) {
		_automation_interval = frames;
	}

	static jack_nframes_t automation_interval() { 
		return _automation_interval;
	}

	virtual void transport_stopped (jack_nframes_t now);
	virtual void automation_snapshot (jack_nframes_t now);

	ARDOUR::Curve& gain_automation_curve () { return _gain_automation_curve; }

	void start_gain_touch ();
	void end_gain_touch ();

	void start_pan_touch (uint32_t which);
	void end_pan_touch (uint32_t which);

	id_t id() const { return _id; }

	void defer_pan_reset ();
	void allow_pan_reset ();

	/* the session calls this for master outs before
	   anyone else. controls outs too, at some point.
	*/

	XMLNode *pending_state_node;
	int ports_became_legal ();

  private:
	mutable PBD::Lock io_lock;

  protected:
	Session&            _session;
	Panner*             _panner;
	gain_t              _gain;
	gain_t              _effective_gain;
	gain_t              _desired_gain;
	PBD::NonBlockingLock declick_lock;
	vector<Port*>       _outputs;
	vector<Port*>       _inputs;
	vector<float>       _peak_power;
	vector<float>       _stored_peak_power;
	string              _name;
	Connection*         _input_connection;
	Connection*         _output_connection;
	id_t                _id;
	bool                 no_panner_reset;
	XMLNode*             deferred_state;

	virtual void set_deferred_state() {}

	void reset_peak_meters();
	void reset_panner ();

	virtual uint32_t pans_required() const { return _ninputs; }

	static void apply_declick (vector<Sample*>&, uint32_t nbufs, jack_nframes_t nframes, 
				   gain_t initial, gain_t target, bool invert_polarity);

	struct MIDIGainControl : public MIDI::Controllable {
	    MIDIGainControl (IO&, MIDI::Port *);
	    void set_value (float);

	    void send_feedback (gain_t);
	    MIDI::byte* write_feedback (MIDI::byte* buf, int32_t& bufsize, gain_t val, bool force = false);

	    IO& io;
	    bool setting;
	    MIDI::byte last_written;

	    gain_t (*midi_to_gain) (double val);
	    double (*gain_to_midi) (gain_t gain);
	};

	MIDIGainControl _midi_gain_control;

	/* state management */

	Change               restore_state (State&);
	StateManager::State* state_factory (std::string why) const;
	void                 send_state_changed();

	bool get_midi_node_info (XMLNode * node, MIDI::eventType & ev, MIDI::channel_t & chan, MIDI::byte & additional);
	bool set_midi_node_info (XMLNode * node, MIDI::eventType ev, MIDI::channel_t chan, MIDI::byte additional);
	
	/* automation */

	jack_nframes_t last_automation_snapshot;
	static jack_nframes_t _automation_interval;

        AutoState      _gain_automation_state;
	AutoStyle      _gain_automation_style;

	bool     apply_gain_automation;
	Curve    _gain_automation_curve;
	
	int  save_automation (const string&);
	int  load_automation (const string&);
	
	PBD::NonBlockingLock automation_lock;

	/* AudioTrack::deprecated_use_diskstream_connections() needs these */

	int set_inputs (const string& str);
	int set_outputs (const string& str);

	static bool connecting_legal;
	static bool ports_legal;

  private:

	uint32_t _ninputs;
	uint32_t _noutputs;

	/* are these the best variable names ever, or what? */

	sigc::connection input_connection_configuration_connection;
	sigc::connection output_connection_configuration_connection;
	sigc::connection input_connection_connection_connection;
	sigc::connection output_connection_connection_connection;

	static bool panners_legal;
	
	int connecting_became_legal ();
	int panners_became_legal ();
	sigc::connection connection_legal_c;
	sigc::connection port_legal_c;
	sigc::connection panner_legal_c;

	int _input_minimum;
	int _input_maximum;
	int _output_minimum;
	int _output_maximum;


	static int parse_io_string (const string&, vector<string>& chns);

	static int parse_gain_string (const string&, vector<string>& chns);
	
	int set_sources (vector<string>&, void *src, bool add);
	int set_destinations (vector<string>&, void *src, bool add);

	int ensure_inputs (uint32_t, bool clear, bool lockit, void *src);
	int ensure_outputs (uint32_t, bool clear, bool lockit, void *src);

	void drop_input_connection ();
	void drop_output_connection ();

	void input_connection_configuration_changed ();
	void input_connection_connection_changed (int);
	void output_connection_configuration_changed ();
	void output_connection_connection_changed (int);

	int create_ports (const XMLNode&);
	int make_connections (const XMLNode&);

	void setup_peak_meters ();
	void grab_peak_power ();

	bool ensure_inputs_locked (uint32_t, bool clear, void *src);
	bool ensure_outputs_locked (uint32_t, bool clear, void *src);

	int32_t find_input_port_hole ();
	int32_t find_output_port_hole ();
};

}; /* namespace ARDOUR */

#endif /*__ardour_io_h__ */
