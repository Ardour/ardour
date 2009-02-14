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
#include <ardour/curve.h>
#include <ardour/types.h>
#include <ardour/data_type.h>

using std::string;
using std::vector;

class XMLNode;

namespace ARDOUR {

class Session;
class AudioEngine;
class Port;
class Connection;
class Panner;

/** A collection of input and output ports with connections.
 *
 * An IO can contain ports of varying types, making routes/inserts/etc with
 * varied combinations of types (eg MIDI and audio) possible.
 */
class IO : public PBD::StatefulDestructible
{

  public:
	static const string state_node_name;

	IO (Session&, string name, 
	    int input_min = -1, int input_max = -1, 
	    int output_min = -1, int output_max = -1,
	    DataType default_type = DataType::AUDIO);

	IO (Session&, const XMLNode&, DataType default_type = DataType::AUDIO);

	virtual ~IO();

	bool active() const { return _active; }
	void set_active (bool yn);

	int input_minimum() const { return _input_minimum; }
	int input_maximum() const { return _input_maximum; }
	int output_minimum() const { return _output_minimum; }
	int output_maximum() const { return _output_maximum; }

	void set_input_minimum (int n);
	void set_input_maximum (int n);
	void set_output_minimum (int n);
	void set_output_maximum (int n);

	DataType default_type() const { return _default_type; }

	const string& name() const { return _name; }
	virtual int set_name (string str, void *src);
	
	virtual void silence  (nframes_t, nframes_t offset);

	// These should be moved in to a separate object that manipulates an IO
	
	void pan (vector<Sample*>& bufs, uint32_t nbufs, nframes_t nframes, nframes_t offset, gain_t gain_coeff);
	void pan_automated (vector<Sample*>& bufs, uint32_t nbufs, nframes_t start_frame, nframes_t end_frame, 
			    nframes_t nframes, nframes_t offset);
	void collect_input  (vector<Sample*>&, uint32_t nbufs, nframes_t nframes, nframes_t offset);
	void deliver_output (vector<Sample*>&, uint32_t nbufs, nframes_t nframes, nframes_t offset);
	void deliver_output_no_pan (vector<Sample*>&, uint32_t nbufs, nframes_t nframes, nframes_t offset);
	void just_meter_input (nframes_t start_frame, nframes_t end_frame, 
			       nframes_t nframes, nframes_t offset);

	virtual uint32_t n_process_buffers () { return 0; }

	virtual void   set_gain (gain_t g, void *src);
	void           inc_gain (gain_t delta, void *src);
	gain_t         gain () const { return _desired_gain; }
	virtual gain_t effective_gain () const;

	Panner& panner() { return *_panner; }
	const Panner& panner() const { return *_panner; }
	
	int ensure_io (uint32_t, uint32_t, bool clear, void *src);

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

	sigc::signal<void>                active_changed;

	sigc::signal<void,IOChange,void*> input_changed;
	sigc::signal<void,IOChange,void*> output_changed;

	sigc::signal<void,void*> gain_changed;
	sigc::signal<void,void*> name_changed;

	virtual XMLNode& state (bool full);
	XMLNode& get_state (void);
	int set_state (const XMLNode&);

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

	PBD::Controllable& gain_control() {
		return _gain_control;
	}

	const PBD::Controllable& gain_control() const {
		return _gain_control;
	}
	
	/* Peak metering */

	float peak_input_power (uint32_t n) { 
		if (n < std::max (_ninputs, _noutputs)) {
			return _visible_peak_power[n];
		} else {
			return minus_infinity();
		}
	}

	float max_peak_power (uint32_t n) {
		if (n < std::max (_ninputs, _noutputs)) {
			return _max_peak_power[n];
		} else {
			return minus_infinity();
		}
	}

	void reset_max_peak_meters ();

	
	static void update_meters();
	static std::string name_from_state (const XMLNode&);
	static void set_name_in_state (XMLNode&, const std::string&);

  private: 

	static sigc::signal<void>   Meter;
	static Glib::StaticMutex    m_meter_signal_lock;
	sigc::connection            m_meter_connection;

  public:

         bool                     _active;

	 /* automation */
	 
	 static void set_automation_interval (jack_nframes_t frames) {
		 _automation_interval = frames;
	 }
	 
	 static jack_nframes_t automation_interval() { 
		 return _automation_interval;
	 }
	 
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
	bool should_snapshot (nframes_t now) {
		return (last_automation_snapshot > now || (now - last_automation_snapshot) > _automation_interval);
	}
	virtual void automation_snapshot (nframes_t now, bool force);

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
	gain_t              _gain;
	gain_t              _effective_gain;
	gain_t              _desired_gain;
	Glib::Mutex         declick_lock;
	vector<Port*>       _outputs;
	vector<Port*>       _inputs;
	vector<float>       _peak_power;
	vector<float>       _visible_peak_power;
	vector<float>       _max_peak_power;
	string              _name;
	Connection*         _input_connection;
	Connection*         _output_connection;
	bool                 no_panner_reset;
	XMLNode*             deferred_state;
	DataType            _default_type;
	bool                _ignore_gain_on_deliver;
	

	virtual void set_deferred_state() {}

	void reset_peak_meters();
	void reset_panner ();

	virtual uint32_t pans_required() const { return _ninputs; }

	static void apply_declick (vector<Sample*>&, uint32_t nbufs, nframes_t nframes, 
				   gain_t initial, gain_t target, bool invert_polarity);

	struct GainControllable : public PBD::Controllable {
	    GainControllable (std::string name, IO& i) : Controllable (name), io (i) {}
	 
	    void set_value (float val);
	    float get_value (void) const;
   
	    IO& io;
	};

	GainControllable _gain_control;

	nframes_t last_automation_snapshot;
	static nframes_t _automation_interval;

	AutoState      _gain_automation_state;
	AutoStyle      _gain_automation_style;

	bool     apply_gain_automation;
	Curve    _gain_automation_curve;
	
	Glib::Mutex automation_lock;

	virtual int set_automation_state (const XMLNode&);
	virtual XMLNode& get_automation_state ();
	virtual int load_automation (std::string path);

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
	Connection *find_possible_connection(const string &desired_name, const string &default_name, const string &connection_type_name);

	void setup_peak_meters ();
	void meter ();

	bool ensure_inputs_locked (uint32_t, bool clear, void *src);
	bool ensure_outputs_locked (uint32_t, bool clear, void *src);

	std::string build_legal_port_name (bool for_input);
	int32_t find_input_port_hole (const char* base);
	int32_t find_output_port_hole (const char* base);
};

} // namespace ARDOUR

#endif /*__ardour_io_h__ */
