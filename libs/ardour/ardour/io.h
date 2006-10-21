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

#include <glibmm/thread.h>

#include <pbd/fastlog.h>
#include <pbd/undo.h>
#include <pbd/statefuldestructible.h> 
#include <pbd/controllable.h>

#include <ardour/ardour.h>
#include <ardour/utils.h>
#include <ardour/state_manager.h>
#include <ardour/curve.h>
#include <ardour/types.h>
#include <ardour/data_type.h>
#include <ardour/port_set.h>
#include <ardour/chan_count.h>

using std::string;
using std::vector;

class XMLNode;

namespace ARDOUR {

class Session;
class AudioEngine;
class Connection;
class Panner;
class PeakMeter;
class Port;
class AudioPort;
class MidiPort;
class BufferSet;

/** A collection of input and output ports with connections.
 *
 * An IO can contain ports of varying types, making routes/inserts/etc with
 * varied combinations of types (eg MIDI and audio) possible.
 */
class IO : public PBD::StatefulDestructible, public ARDOUR::StateManager
{

  public:
	static const string state_node_name;

	IO (Session&, string name, 
	    int input_min = -1, int input_max = -1, 
	    int output_min = -1, int output_max = -1,
		DataType default_type = DataType::AUDIO);
	
	virtual ~IO();

	ChanCount input_minimum() const { return _input_minimum; }
	ChanCount input_maximum() const { return _input_maximum; }
	ChanCount output_minimum() const { return _output_minimum; }
	ChanCount output_maximum() const { return _output_maximum; }

	void set_input_minimum (ChanCount n);
	void set_input_maximum (ChanCount n);
	void set_output_minimum (ChanCount n);
	void set_output_maximum (ChanCount n);
	
	// Do not write any new code using these
	void set_input_minimum (int n);
	void set_input_maximum (int n);
	void set_output_minimum (int n);
	void set_output_maximum (int n);

	DataType default_type() const         { return _default_type; }
	void     set_default_type(DataType t) { _default_type = t; }

	const string& name() const { return _name; }
	virtual int set_name (string str, void *src);
	
	virtual void silence  (nframes_t, nframes_t offset);

	void collect_input  (BufferSet& bufs, jack_nframes_t nframes, jack_nframes_t offset);
	void deliver_output (BufferSet& bufs, jack_nframes_t start_frame, jack_nframes_t end_frame,
	                                      jack_nframes_t nframes, jack_nframes_t offset);
	void just_meter_input (jack_nframes_t start_frame, jack_nframes_t end_frame, 
			       jack_nframes_t nframes, jack_nframes_t offset);

	virtual void   set_gain (gain_t g, void *src);
	void           inc_gain (gain_t delta, void *src);
	gain_t         gain () const { return _desired_gain; }
	virtual gain_t effective_gain () const;
	
	void set_phase_invert (bool yn, void *src);
	bool phase_invert() const { return _phase_invert; }

	Panner& panner()        { return *_panner; }
	PeakMeter& peak_meter() { return *_meter; }

	int ensure_io (ChanCount in, ChanCount out, bool clear, void *src);

	int use_input_connection (Connection&, void *src);
	int use_output_connection (Connection&, void *src);

	Connection *input_connection() const { return _input_connection; }
	Connection *output_connection() const { return _output_connection; }

	int add_input_port (string source, void *src, DataType type = DataType::NIL);
	int add_output_port (string destination, void *src, DataType type = DataType::NIL);

	int remove_input_port (Port *, void *src);
	int remove_output_port (Port *, void *src);

	int set_input (Port *, void *src);

	int connect_input (Port *our_port, string other_port, void *src);
	int connect_output (Port *our_port, string other_port, void *src);

	int disconnect_input (Port *our_port, string other_port, void *src);
	int disconnect_output (Port *our_port, string other_port, void *src);

	int disconnect_inputs (void *src);
	int disconnect_outputs (void *src);

	nframes_t output_latency() const;
	nframes_t input_latency() const;
	void           set_port_latency (nframes_t);

	const PortSet& inputs()  const { return _inputs; }
	const PortSet& outputs() const { return _outputs; }

	Port *output (uint32_t n) const {
		if (n < _outputs.num_ports()) {
			return _outputs.port(n);
		} else {
			return 0;
		}
	}

	Port *input (uint32_t n) const {
		if (n < _inputs.num_ports()) {
			return _inputs.port(n);
		} else {
			return 0;
		}
	}

	AudioPort* audio_input(uint32_t n) const;
	AudioPort* audio_output(uint32_t n) const;
	MidiPort*  midi_input(uint32_t n) const;
	MidiPort*  midi_output(uint32_t n) const;

	const ChanCount& n_inputs ()  const { return _inputs.count(); }
	const ChanCount& n_outputs () const { return _outputs.count(); }

	void attach_buffers(ChanCount ignored);

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
	
	static sigc::signal<int>            PortsLegal;
	static sigc::signal<int>            PannersLegal;
	static sigc::signal<int>            ConnectingLegal;
	static sigc::signal<void,ChanCount> MoreChannels;
	static sigc::signal<int>            PortsCreated;

	PBD::Controllable& gain_control() {
		return _gain_control;
	}

    static void update_meters();

private: 

    static sigc::signal<void>   Meter;
    static Glib::StaticMutex    m_meter_signal_lock;
    sigc::connection            m_meter_connection;

public:

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

	virtual void transport_stopped (nframes_t now);

	ARDOUR::Curve& gain_automation_curve () { return _gain_automation_curve; }

	void start_gain_touch ();
	void end_gain_touch ();

	void start_pan_touch (uint32_t which);
	void end_pan_touch (uint32_t which);

	void defer_pan_reset ();
	void allow_pan_reset ();

	/* the session calls this for master outs before
	   anyone else. controls outs too, at some point.
	*/

	XMLNode *pending_state_node;
	int ports_became_legal ();

  private:
	mutable Glib::Mutex io_lock;

  protected:
	Session&            _session;
	Panner*             _panner;
	BufferSet*          _output_buffers; //< Set directly to output port buffers
	gain_t              _gain;
	gain_t              _effective_gain;
	gain_t              _desired_gain;
	Glib::Mutex         declick_lock;
	PortSet             _outputs;
	PortSet             _inputs;
	PeakMeter*          _meter;
	string              _name;
	Connection*         _input_connection;
	Connection*         _output_connection;
	bool                 no_panner_reset;
	bool                _phase_invert;
	XMLNode*             deferred_state;
	DataType            _default_type;
	
	virtual void set_deferred_state() {}

	void reset_panner ();

	virtual uint32_t pans_required() const
		{ return _inputs.count().get(DataType::AUDIO); }

	struct GainControllable : public PBD::Controllable {
	    GainControllable (std::string name, IO& i) : Controllable (name), io (i) {}
	 
	    void set_value (float val);
	    float get_value (void) const;
   
	    IO& io;
	};

	GainControllable _gain_control;

	/* state management */

	Change               restore_state (State&);
	StateManager::State* state_factory (std::string why) const;

	AutoState      _gain_automation_state;
	AutoStyle      _gain_automation_style;

	bool     apply_gain_automation;
	Curve    _gain_automation_curve;
	
	int  save_automation (const string&);
	int  load_automation (const string&);
	
	Glib::Mutex automation_lock;

	/* AudioTrack::deprecated_use_diskstream_connections() needs these */

	int set_inputs (const string& str);
	int set_outputs (const string& str);

	static bool connecting_legal;
	static bool ports_legal;

	BufferSet& output_buffers() { return *_output_buffers; }

  private:

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

	ChanCount _input_minimum;
	ChanCount _input_maximum;
	ChanCount _output_minimum;
	ChanCount _output_maximum;


	static int parse_io_string (const string&, vector<string>& chns);

	static int parse_gain_string (const string&, vector<string>& chns);
	
	int set_sources (vector<string>&, void *src, bool add);
	int set_destinations (vector<string>&, void *src, bool add);

	int ensure_inputs (ChanCount, bool clear, bool lockit, void *src);
	int ensure_outputs (ChanCount, bool clear, bool lockit, void *src);

	void drop_input_connection ();
	void drop_output_connection ();

	void input_connection_configuration_changed ();
	void input_connection_connection_changed (int);
	void output_connection_configuration_changed ();
	void output_connection_connection_changed (int);

	int create_ports (const XMLNode&);
	int make_connections (const XMLNode&);

	void setup_peak_meters ();
	void meter ();

	bool ensure_inputs_locked (ChanCount, bool clear, void *src);
	bool ensure_outputs_locked (ChanCount, bool clear, void *src);

	int32_t find_input_port_hole ();
	int32_t find_output_port_hole ();
};

} // namespace ARDOUR

#endif /*__ardour_io_h__ */
