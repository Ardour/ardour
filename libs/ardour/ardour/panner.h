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

    $Id$
*/

#ifndef __ardour_panner_h__
#define __ardour_panner_h__

#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <sigc++/signal.h>

#include <pbd/stateful.h> 
#include <pbd/controllable.h>

#include <ardour/types.h>
#include <ardour/curve.h>

using std::istream;
using std::ostream;

namespace ARDOUR {

class Session;
class Panner;

class StreamPanner : public sigc::trackable, public Stateful
{
  public:
	StreamPanner (Panner& p);
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

	/* the basic panner API */

	virtual void distribute (Sample* src, Sample** obufs, gain_t gain_coeff, nframes_t nframes) = 0;
	virtual void distribute_automated (Sample* src, Sample** obufs, 
				     nframes_t start, nframes_t end, nframes_t nframes, pan_t** buffers) = 0;

	/* automation */

	virtual void snapshot (nframes_t now) = 0;
	virtual void transport_stopped (nframes_t frame) = 0;
	virtual void set_automation_state (AutoState) = 0;
	virtual void set_automation_style (AutoStyle) = 0;
	
	PBD::Controllable& control()  { return _control; }
	
	/* XXX this is wrong. for multi-dimensional panners, there
	   must surely be more than 1 automation curve.
	*/

	virtual Curve& automation() = 0;

	virtual int load (istream&, string path, uint32_t&) = 0;

	virtual int save (ostream&) const = 0;

	sigc::signal<void> Changed;      /* for position */
	sigc::signal<void> StateChanged; /* for mute */

	int set_state (const XMLNode&);
	virtual XMLNode& state (bool full_state) = 0;

	Panner & get_parent() { return parent; }
	
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

	struct PanControllable : public PBD::Controllable {
	    PanControllable (StreamPanner& p) : panner (p) {}
	    
	    StreamPanner& panner;
	    
	    void set_value (float);
	    float get_value (void) const;
	    bool can_send_feedback() const;
	};

	PanControllable  _control;

	void add_state (XMLNode&);
	virtual void update () = 0;
};

class BaseStereoPanner : public StreamPanner
{
  public:
	BaseStereoPanner (Panner&);
	~BaseStereoPanner ();

	/* this class just leaves the pan law itself to be defined
	   by the update(), distribute_automated() 
	   methods. derived classes also need a factory method
	   and a type name. See EqualPowerStereoPanner as an example.
	*/

	void distribute (Sample* src, Sample** obufs, gain_t gain_coeff, nframes_t nframes);

	int load (istream&, string path, uint32_t&);
	int save (ostream&) const;
	void snapshot (nframes_t now);
	void transport_stopped (nframes_t frame);
	void set_automation_state (AutoState);
	void set_automation_style (AutoStyle);

	Curve& automation() { return _automation; }

  protected:
	float left;
	float right;
	float desired_left;
	float desired_right;
	float left_interp;
	float right_interp;

	Curve  _automation;
};

class EqualPowerStereoPanner : public BaseStereoPanner
{
  public:
	EqualPowerStereoPanner (Panner&);
	~EqualPowerStereoPanner ();

	void distribute_automated (Sample* src, Sample** obufs, 
			     nframes_t start, nframes_t end, nframes_t nframes, pan_t** buffers);

	void get_current_coefficients (pan_t*) const;
	void get_desired_coefficients (pan_t*) const;

	static StreamPanner* factory (Panner&);
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
	Multi2dPanner (Panner& parent);
	~Multi2dPanner ();

	void snapshot (nframes_t now);
	void transport_stopped (nframes_t frame);
	void set_automation_state (AutoState);
	void set_automation_style (AutoStyle);

	/* XXX this is wrong. for multi-dimensional panners, there
	   must surely be more than 1 automation curve.
	*/

	Curve& automation() { return _automation; }

	void distribute (Sample* src, Sample** obufs, gain_t gain_coeff, nframes_t nframes);
	void distribute_automated (Sample* src, Sample** obufs, 
			     nframes_t start, nframes_t end, nframes_t nframes, pan_t** buffers);

	int load (istream&, string path, uint32_t&);
	int save (ostream&) const;

	static StreamPanner* factory (Panner&);
	static string name;

	XMLNode& state (bool full_state); 
	XMLNode& get_state (void);
	int set_state (const XMLNode&);

  private:
	Curve _automation;
	void update ();
};

class Panner : public std::vector<StreamPanner*>, public Stateful, public sigc::trackable
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

	void set_name (string);

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

	int load ();
	int save () const;

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
	
  private:

	string            automation_path;
	Session&         _session;
	uint32_t     current_outs;
	bool             _linked;
	bool             _bypassed;
	LinkDirection    _link_direction;

	static float current_automation_version_number;
};

} // namespace ARDOUR

#endif /*__ardour_panner_h__ */
