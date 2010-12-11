/*
    Copyright (C) 2004 Paul Davis

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

#ifndef __ardour_panner_h__
#define __ardour_panner_h__

#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <iostream>

#include "pbd/stateful.h"
#include "pbd/controllable.h"
#include "pbd/cartesian.h"

#include "ardour/types.h"
#include "ardour/automation_control.h"
#include "ardour/processor.h"

namespace ARDOUR {

class Session;
class Panner;
class BufferSet;
class AudioBuffer;
class Speakers;

class StreamPanner : public PBD::Stateful
{
  public:
	StreamPanner (Panner& p, Evoral::Parameter param);
	~StreamPanner ();

	void set_muted (bool yn);
	bool muted() const { return _muted; }

	const PBD::AngularVector& get_position() const { return _angles; }
	const PBD::AngularVector& get_effective_position() const { return _effective_angles; }
	void set_position (const PBD::AngularVector&, bool link_call = false);
	void set_diffusion (double);

	void distribute (AudioBuffer &, BufferSet &, gain_t, pframes_t);
	void distribute_automated (AudioBuffer &, BufferSet &, framepos_t, framepos_t, pframes_t, pan_t **);

	/* the basic StreamPanner API */

	/**
	 *  Pan some input samples to a number of output buffers.
	 *
	 *  @param src Input buffer.
	 *  @param obufs Output buffers (one per panner output).
	 *  @param gain_coeff Gain coefficient to apply to output samples.
	 *  @param nframes Number of frames in the input.
	 */
	virtual void do_distribute (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes) = 0;
	virtual void do_distribute_automated (AudioBuffer& src, BufferSet& obufs,
	                                      framepos_t start, framepos_t end, pframes_t nframes,
	                                      pan_t** buffers) = 0;

	boost::shared_ptr<AutomationControl> pan_control()  { return _control; }

	PBD::Signal0<void> Changed;      /* for position or diffusion */
	PBD::Signal0<void> StateChanged; /* for mute, mono */

	int set_state (const XMLNode&, int version);
	virtual XMLNode& state (bool full_state) = 0;

	Panner & get_parent() { return parent; }

	/* old school automation loading */
	virtual int load (std::istream&, std::string path, uint32_t&) = 0;

	struct PanControllable : public AutomationControl {
		PanControllable (Session& s, std::string name, StreamPanner* p, Evoral::Parameter param)
			: AutomationControl (s, param,
                                             boost::shared_ptr<AutomationList>(new AutomationList(param)), name)
			, streampanner (p)
		{ assert (param.type() == PanAutomation); }
                
		AutomationList* alist() { return (AutomationList*)_list.get(); }
		StreamPanner* streampanner;

		void set_value (double);
		double get_value (void) const;
                double lower () const;
	};

  protected:
	friend class Panner;
	Panner& parent;

	void set_mono (bool);
	
	PBD::AngularVector _angles;
	PBD::AngularVector _effective_angles;
	double        _diffusion; 

	bool _muted;
	bool _mono;
        
	boost::shared_ptr<AutomationControl> _control;

	XMLNode& get_state ();

	/* Update internal parameters based on this.angles */
	virtual void update () = 0;
};

class BaseStereoPanner : public StreamPanner
{
  public:
	BaseStereoPanner (Panner&, Evoral::Parameter param);
	~BaseStereoPanner ();

	/* this class just leaves the pan law itself to be defined
	   by the update(), do_distribute_automated()
	   methods. derived classes also need a factory method
	   and a type name. See EqualPowerStereoPanner as an example.
	*/

	void do_distribute (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes);

	static double azimuth_to_lr_fract (double azi) { 
		/* 180.0 degrees=> left => 0.0 */
		/* 0.0 degrees => right => 1.0 */
		return 1.0 - (azi/180.0);
	}

	static double lr_fract_to_azimuth (double fract) { 
		/* fract = 0.0 => degrees = 180.0 => left */
		/* fract = 1.0 => degrees = 0.0 => right */
		return 180.0 - (fract * 180.0);
	}
	
	/* old school automation loading */

	int load (std::istream&, std::string path, uint32_t&);

  protected:
	float left;
	float right;
	float desired_left;
	float desired_right;
	float left_interp;
	float right_interp;
};

class EqualPowerStereoPanner : public BaseStereoPanner
{
  public:
	EqualPowerStereoPanner (Panner&, Evoral::Parameter param);
	~EqualPowerStereoPanner ();

	void do_distribute_automated (AudioBuffer& src, BufferSet& obufs,
	                              framepos_t start, framepos_t end, pframes_t nframes,
	                              pan_t** buffers);

	void get_current_coefficients (pan_t*) const;
	void get_desired_coefficients (pan_t*) const;

	static StreamPanner* factory (Panner&, Evoral::Parameter param, Speakers&);
	static std::string name;

	XMLNode& state (bool full_state); 
	XMLNode& get_state (void); 
	int      set_state (const XMLNode&, int version);

  private:
	void update ();
};

/** Class to pan from some number of inputs to some number of outputs.
 *  This class has a number of StreamPanners, one for each input.
 */
class Panner : public SessionObject, public Automatable
{
public:
	struct Output {
            PBD::AngularVector position;
            pan_t current_pan;
            pan_t desired_pan;
            
            Output (const PBD::AngularVector& a) 
            : position (a), current_pan (0), desired_pan (0) {}

	};

	Panner (std::string name, Session&);
	virtual ~Panner ();

	void clear_panners ();
	bool empty() const { return _streampanners.empty(); }

	void set_automation_state (AutoState);
	AutoState automation_state() const;
	void set_automation_style (AutoStyle);
	AutoStyle automation_style() const;
	bool touching() const;

	std::string describe_parameter (Evoral::Parameter param);

	bool can_support_io_configuration (const ChanCount& /*in*/, ChanCount& /*out*/) const { return true; };

	/// The fundamental Panner function
	void run (BufferSet& src, BufferSet& dest, framepos_t start_frame, framepos_t end_frames, pframes_t nframes);

	bool bypassed() const { return _bypassed; }
	void set_bypassed (bool yn);
	bool mono () const { return _mono; }
	void set_mono (bool);

	StreamPanner* add ();
	void remove (uint32_t which);
	void reset (uint32_t noutputs, uint32_t npans);
	void reset_streampanner (uint32_t which_panner);
	void reset_to_default ();

	XMLNode& get_state (void);
	XMLNode& state (bool full);
	int      set_state (const XMLNode&, int version);

	static bool equivalent (pan_t a, pan_t b) {
		return fabsf (a - b) < 0.002; // about 1 degree of arc for a stereo panner
	}
	static bool equivalent (const PBD::AngularVector& a, const PBD::AngularVector& b) {
                /* XXX azimuth only, at present */
		return fabs (a.azi - b.azi) < 1.0;
	}

	void move_output (uint32_t, float x, float y);
	uint32_t nouts() const { return outputs.size(); }
	Output& output (uint32_t n) { return outputs[n]; }

	enum LinkDirection {
		SameDirection,
		OppositeDirection
	};

	LinkDirection link_direction() const { return _link_direction; }
	void set_link_direction (LinkDirection);

	bool linked() const { return _linked; }
	void set_linked (bool yn);

	StreamPanner &streampanner( uint32_t n ) const { assert( n < _streampanners.size() ); return *_streampanners[n]; }
	uint32_t npanners() const { return _streampanners.size(); }

	PBD::Signal0<void> Changed; /* panner and/or outputs count changed */
	PBD::Signal0<void> LinkStateChanged;
	PBD::Signal0<void> StateChanged; /* for bypass */

	/* only StreamPanner should call these */

	void set_position (const PBD::AngularVector&, StreamPanner& orig);

	/* old school automation */

	int load ();

	boost::shared_ptr<AutomationControl> pan_control (int id, uint32_t chan=0) {
		return automation_control (Evoral::Parameter (PanAutomation, chan, id));
	}

	boost::shared_ptr<const AutomationControl> pan_control (int id, uint32_t chan=0) const {
		return automation_control (Evoral::Parameter (PanAutomation, chan, id));
	}

	boost::shared_ptr<AutomationControl> direction_control () {
		return automation_control (Evoral::Parameter (PanAutomation, 0, 100));
	}

	boost::shared_ptr<AutomationControl> width_control () {
		return automation_control (Evoral::Parameter (PanAutomation, 0, 200));
	}

        void set_stereo_position (double);
        void set_stereo_width (double);
        bool set_stereo_pan (double pos, double width);
        
	static std::string value_as_string (double);
        
  private:
	/* disallow copy construction */
	Panner (Panner const &);

	void distribute_no_automation(BufferSet& src, BufferSet& dest, pframes_t nframes, gain_t gain_coeff);
	std::vector<StreamPanner*> _streampanners; ///< one StreamPanner per input
	std::vector<Output> outputs;
	uint32_t     current_outs;
	bool             _linked;
	bool             _bypassed;
	bool             _mono;
	LinkDirection    _link_direction;

	static float current_automation_version_number;

	void setup_speakers (uint32_t nouts);
        void setup_meta_controls ();

	/* old school automation handling */

	std::string automation_path;
};

} // namespace ARDOUR

#endif /*__ardour_panner_h__ */
