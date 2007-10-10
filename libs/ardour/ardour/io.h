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
#include <ardour/automatable.h>
#include <ardour/utils.h>
#include <ardour/curve.h>
#include <ardour/types.h>
#include <ardour/data_type.h>
#include <ardour/port_set.h>
#include <ardour/chan_count.h>
#include <ardour/latent.h>
#include <ardour/automation_control.h>

using std::string;
using std::vector;

class XMLNode;

namespace ARDOUR {

class Session;
class AudioEngine;
class Bundle;
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

class IO : public Automatable, public Latent
{
  public:
	static const string state_node_name;

	IO (Session&, const string& name, 
	    int input_min = -1, int input_max = -1, 
	    int output_min = -1, int output_max = -1,
	    DataType default_type = DataType::AUDIO);
	
	IO (Session&, const XMLNode&, DataType default_type = DataType::AUDIO);
	
	virtual ~IO();

	ChanCount input_minimum() const { return _input_minimum; }
	ChanCount input_maximum() const { return _input_maximum; }
	ChanCount output_minimum() const { return _output_minimum; }
	ChanCount output_maximum() const { return _output_maximum; }
	
	void set_input_minimum (ChanCount n);
	void set_input_maximum (ChanCount n);
	void set_output_minimum (ChanCount n);
	void set_output_maximum (ChanCount n);
	
	DataType default_type() const         { return _default_type; }
	void     set_default_type(DataType t) { _default_type = t; }
	
	bool set_name (const string& str);

	virtual void silence  (nframes_t, nframes_t offset);

	void collect_input  (BufferSet& bufs, nframes_t nframes, nframes_t offset);
	void deliver_output (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame,
	                                      nframes_t nframes, nframes_t offset);
	void just_meter_input (nframes_t start_frame, nframes_t end_frame, 
			       nframes_t nframes, nframes_t offset);

	gain_t         gain () const { return _desired_gain; }
	virtual gain_t effective_gain () const;
	
	void set_denormal_protection (bool yn, void *src);
	bool denormal_protection() const { return _denormal_protection; }
	
	void set_phase_invert (bool yn, void *src);
	bool phase_invert() const { return _phase_invert; }

	Panner& panner()        { return *_panner; }
	PeakMeter& peak_meter() { return *_meter; }
	const Panner& panner() const { return *_panner; }
	
	int ensure_io (ChanCount in, ChanCount out, bool clear, void *src);

	int connect_input_ports_to_bundle (boost::shared_ptr<Bundle>, void *src);
	int connect_output_ports_to_bundle (boost::shared_ptr<Bundle>, void *src);

	boost::shared_ptr<Bundle> input_bundle();
	boost::shared_ptr<Bundle> output_bundle();

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

	nframes_t signal_latency() const { return _own_latency; }
	nframes_t output_latency() const;
	nframes_t input_latency() const;
	void      set_port_latency (nframes_t);

	void update_port_total_latencies ();

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

        boost::shared_ptr<Bundle> bundle_for_inputs () const { return _bundle_for_inputs; }
        boost::shared_ptr<Bundle> bundle_for_outputs () const { return _bundle_for_outputs; }

	sigc::signal<void,IOChange,void*> input_changed;
	sigc::signal<void,IOChange,void*> output_changed;

	virtual XMLNode& state (bool full);
	XMLNode& get_state (void);
	int set_state (const XMLNode&);

	static int  disable_connecting (void);

	static int  enable_connecting (void);

	static int  disable_ports (void);

	static int  enable_ports (void);

	static int  disable_panners (void);

	static int  reset_panners (void);
	
	static sigc::signal<int>            PortsLegal;
	static sigc::signal<int>            PannersLegal;
	static sigc::signal<int>            ConnectingLegal;
	/// raised when the number of input or output ports changes
	static sigc::signal<void,ChanCount> PortCountChanged;
	static sigc::signal<int>            PortsCreated;

    static void update_meters();

  private: 

    static sigc::signal<void>   Meter;
    static Glib::StaticMutex    m_meter_signal_lock;
    sigc::connection            m_meter_connection;

  public:

	/* automation */
	
	struct GainControl : public AutomationControl {
	    GainControl (std::string name, IO& i, boost::shared_ptr<AutomationList> al)
			: AutomationControl (i._session, al, name)
			, _io (i)
		{}
	 
	    void set_value (float val);
	    float get_value (void) const;
   
	    IO& _io;
	};

	boost::shared_ptr<GainControl> gain_control() {
		return _gain_control;
	}
	boost::shared_ptr<const GainControl> gain_control() const {
		return _gain_control;
	}

	void clear_automation ();
	
	void set_parameter_automation_state (Parameter, AutoState);

	virtual void transport_stopped (nframes_t now); // interface: matches Insert
	void automation_snapshot (nframes_t now); // interface: matches Automatable

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
	Panner*             _panner;
	BufferSet*          _output_buffers; //< Set directly to output port buffers
	gain_t              _gain;
	gain_t              _effective_gain;
	gain_t              _desired_gain;
	Glib::Mutex         declick_lock;
	PortSet             _outputs;
	PortSet             _inputs;
	PeakMeter*          _meter;
	boost::shared_ptr<Bundle> _input_bundle; ///< bundle connected to our inputs
	boost::shared_ptr<Bundle> _output_bundle; ///< bundle connected to our outputs
	bool                 no_panner_reset;
	bool                _phase_invert;
	bool                _denormal_protection;
	XMLNode*             deferred_state;
	DataType            _default_type;
	
	virtual void set_deferred_state() {}

	void reset_panner ();

	virtual uint32_t pans_required() const
		{ return _inputs.count().n_audio(); }

	boost::shared_ptr<GainControl> _gain_control;

	virtual void   set_gain (gain_t g, void *src);
	void           inc_gain (gain_t delta, void *src);

	bool apply_gain_automation;
	
	virtual int load_automation (std::string path);

	/* AudioTrack::deprecated_use_diskstream_connections() needs these */

	int set_inputs (const string& str);
	int set_outputs (const string& str);

	static bool connecting_legal;
	static bool ports_legal;

	BufferSet& output_buffers() { return *_output_buffers; }

  private:

	friend class Send;

	/* are these the best variable names ever, or what? */

	sigc::connection input_bundle_configuration_connection;
	sigc::connection output_bundle_configuration_connection;
	sigc::connection input_bundle_connection_connection;
	sigc::connection output_bundle_connection_connection;

	static bool panners_legal;
	
	int connecting_became_legal ();
	int panners_became_legal ();
	sigc::connection connection_legal_c;
	sigc::connection port_legal_c;
	sigc::connection panner_legal_c;

	ChanCount _input_minimum; ///< minimum number of input channels (0 for no minimum)
	ChanCount _input_maximum; ///< maximum number of input channels (ChanCount::INFINITE for no maximum)
	ChanCount _output_minimum; ///< minimum number of output channels (0 for no minimum)
	ChanCount _output_maximum; ///< maximum number of output channels (ChanCount::INFINITE for no maximum)

	boost::shared_ptr<Bundle> _bundle_for_inputs;
	boost::shared_ptr<Bundle> _bundle_for_outputs;

	static int parse_io_string (const string&, vector<string>& chns);

	static int parse_gain_string (const string&, vector<string>& chns);
	
	int set_sources (vector<string>&, void *src, bool add);
	int set_destinations (vector<string>&, void *src, bool add);

	int ensure_inputs (ChanCount, bool clear, bool lockit, void *src);
	int ensure_outputs (ChanCount, bool clear, bool lockit, void *src);

	void drop_input_bundle ();
	void drop_output_bundle ();

	void input_bundle_configuration_changed ();
	void input_bundle_connection_changed (int);
	void output_bundle_configuration_changed ();
	void output_bundle_connection_changed (int);

	int create_ports (const XMLNode&);
	int make_connections (const XMLNode&);

	void setup_peak_meters ();
	void meter ();

	bool ensure_inputs_locked (ChanCount, bool clear, void *src);
	bool ensure_outputs_locked (ChanCount, bool clear, void *src);

	int32_t find_input_port_hole ();
	int32_t find_output_port_hole ();

	void create_bundles ();
	void setup_bundles ();
};

} // namespace ARDOUR

#endif /*__ardour_io_h__ */
