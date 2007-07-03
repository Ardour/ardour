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
#include <sigc++/signal.h>

#include <pbd/stateful.h> 
#include <pbd/controllable.h>

#include <ardour/types.h>
#include <ardour/curve.h>
#include <ardour/automation_control.h>

using std::istream;
using std::ostream;

namespace ARDOUR {

class Session;
class Panner;
class BufferSet;
class AudioBuffer;

class StreamPanner : public sigc::trackable, public PBD::Stateful
{
  public:
	StreamPanner (Panner& p, Parameter param);
	~StreamPanner ();

	void set_muted (bool yn);
	bool muted() const { return _muted; }

	void set_position (float x, bool link_call = false);
	void set_position (float x, float y, bool link_call = false);
	void set_position (float x, float y, float z, bool link_call = false);

	void get_position (float& xpos) const { xpos = x; }
	void get_position (float& xpos, float& ypos) const { xpos = x; ypos = y; }
	void get_position (float& xpos, float& ypos, float& zpos) const { xpos = x; ypos = y; zpos = z; }

	void get_effective_position (float& xpos) const { xpos = effective_x; }
	void get_effective_position (float& xpos, float& ypos) const { xpos = effective_x; ypos = effective_y; }
	void get_effective_position (float& xpos, float& ypos, float& zpos) const { xpos = effective_x; ypos = effective_y; zpos = effective_z; }

	/* the basic StreamPanner API */

	virtual void distribute (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, nframes_t nframes) = 0;
	virtual void distribute_automated (AudioBuffer& src, BufferSet& obufs,
				     nframes_t start, nframes_t end, nframes_t nframes, pan_t** buffers) = 0;

	boost::shared_ptr<AutomationControl> pan_control()  { return _control; }
	
	sigc::signal<void> Changed;      /* for position */
	sigc::signal<void> StateChanged; /* for mute */

	int set_state (const XMLNode&);
	virtual XMLNode& state (bool full_state) = 0;
	
	Panner & get_parent() { return parent; }

	/* old school automation loading */

	virtual int load (istream&, string path, uint32_t&) = 0;

  protected:
	friend class Panner;
	Panner& parent;

	float x;
	float y;
	float z;
	
	/* these are for automation. they store the last value
	   used by the most recent process() cycle.
	*/

	float effective_x;
	float effective_y;
	float effective_z;

	bool             _muted;

	struct PanControllable : public AutomationControl {
	    PanControllable (Session& s, std::string name, StreamPanner& p, Parameter param)
			: AutomationControl (s, boost::shared_ptr<AutomationList>(new AutomationList(
					param, 0.0, 1.0, 0.5)), name)
			, panner (p) { assert(param.type() != NullAutomation); }
	    
	    StreamPanner& panner;
	    
	    void set_value (float);
	    float get_value (void) const;
	    bool can_send_feedback() const;
	};

	boost::shared_ptr<PanControllable> _control;

	void add_state (XMLNode&);
	virtual void update () = 0;
};

class BaseStereoPanner : public StreamPanner
{
  public:
	BaseStereoPanner (Panner&, Parameter param);
	~BaseStereoPanner ();

	/* this class just leaves the pan law itself to be defined
	   by the update(), distribute_automated() 
	   methods. derived classes also need a factory method
	   and a type name. See EqualPowerStereoPanner as an example.
	*/

	void distribute (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, nframes_t nframes);

	/* old school automation loading */

	int load (istream&, string path, uint32_t&);

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
	EqualPowerStereoPanner (Panner&, Parameter param);
	~EqualPowerStereoPanner ();

	void distribute_automated (AudioBuffer& src, BufferSet& obufs, 
			     nframes_t start, nframes_t end, nframes_t nframes, pan_t** buffers);

	void get_current_coefficients (pan_t*) const;
	void get_desired_coefficients (pan_t*) const;

	static StreamPanner* factory (Panner&, Parameter param);
	static string name;

	XMLNode& state (bool full_state); 
	XMLNode& get_state (void); 
	int      set_state (const XMLNode&);

  private:
	void update ();
};

class Multi2dPanner : public StreamPanner
{
  public:
	Multi2dPanner (Panner& parent, Parameter);
	~Multi2dPanner ();

	void distribute (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, nframes_t nframes);
	void distribute_automated (AudioBuffer& src, BufferSet& obufs,
				   nframes_t start, nframes_t end, nframes_t nframes, pan_t** buffers);

	static StreamPanner* factory (Panner&, Parameter);
	static string name;

	XMLNode& state (bool full_state); 
	XMLNode& get_state (void);
	int set_state (const XMLNode&);

	/* old school automation loading */

	int load (istream&, string path, uint32_t&);

  private:
	void update ();
};

class Panner : public std::vector<StreamPanner*>, public PBD::Stateful, public sigc::trackable
{
  public:
	struct Output {
	    float x;
	    float y;
	    pan_t current_pan;
	    pan_t desired_pan;

	    Output (float xp, float yp) 
		    : x (xp), y (yp), current_pan (0.0f), desired_pan (0.f) {}
		    
	};

	Panner (string name, Session&);
	virtual ~Panner ();

	/// The fundamental Panner function
	void distribute(BufferSet& src, BufferSet& dest, nframes_t start_frame, nframes_t end_frames, nframes_t nframes, nframes_t offset);

	bool bypassed() const { return _bypassed; }
	void set_bypassed (bool yn);

	StreamPanner* add ();
	void remove (uint32_t which);
	void clear ();
	void reset (uint32_t noutputs, uint32_t npans);

	void snapshot (nframes_t now);
	void transport_stopped (nframes_t frame);
	
	void clear_automation ();

	void set_automation_state (AutoState);
	AutoState automation_state() const;
	void set_automation_style (AutoStyle);
	AutoStyle automation_style() const;
	bool touching() const;

	XMLNode& get_state (void);
	XMLNode& state (bool full);
	int      set_state (const XMLNode&);

	sigc::signal<void> Changed;
	
	static bool equivalent (pan_t a, pan_t b) {
		return fabsf (a - b) < 0.002; // about 1 degree of arc for a stereo panner
	}

	void move_output (uint32_t, float x, float y);
	uint32_t nouts() const { return outputs.size(); }
	Output& output (uint32_t n) { return outputs[n]; }

	std::vector<Output> outputs;
	Session& session() const { return _session; }

	enum LinkDirection {
		SameDirection,
		OppositeDirection
	};

	LinkDirection link_direction() const { return _link_direction; }
	void set_link_direction (LinkDirection);
	
	bool linked() const { return _linked; }
	void set_linked (bool yn);

	sigc::signal<void> LinkStateChanged;
	sigc::signal<void> StateChanged; /* for bypass */

	/* only StreamPanner should call these */
	
	void set_position (float x, StreamPanner& orig);
	void set_position (float x, float y, StreamPanner& orig);
	void set_position (float x, float y, float z, StreamPanner& orig);

	/* old school automation */

	int load ();

  private:
	void distribute_no_automation(BufferSet& src, BufferSet& dest, nframes_t nframes, nframes_t offset, gain_t gain_coeff);

	Session&         _session;
	uint32_t     current_outs;
	bool             _linked;
	bool             _bypassed;
	LinkDirection    _link_direction;

	static float current_automation_version_number;

	/* old school automation handling */

	std::string automation_path;
	void set_name (std::string);
};

} // namespace ARDOUR

#endif /*__ardour_panner_h__ */
