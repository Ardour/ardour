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

#ifndef __ardour_overlap_h__
#define __ardour_overlap_h__

#include <vector>
#include <algorithm>

#include <sigc++/signal.h>

#include <pbd/undo.h>

#include <ardour/ardour.h>
#include <ardour/curve.h>
#include <ardour/audioregion.h>
#include <ardour/state_manager.h>
#include <ardour/crossfade_compare.h>

namespace ARDOUR {

class AudioRegion;
class Playlist;

struct CrossfadeState : public StateManager::State {
    CrossfadeState (std::string reason) : StateManager::State (reason) {}

    UndoAction fade_in_memento;
    UndoAction fade_out_memento;
    jack_nframes_t position;
    jack_nframes_t length;
    AnchorPoint    anchor_point;
    bool           follow_overlap;
    bool           active;
};

class Crossfade : public Stateful, public StateManager
{
  public:

	class NoCrossfadeHere: std::exception {
	  public:
		virtual const char *what() const throw() { return "no crossfade should be constructed here"; }
	};
	
	/* constructor for "fixed" xfades at each end of an internal overlap */

	Crossfade (ARDOUR::AudioRegion& in, ARDOUR::AudioRegion& out,
		   jack_nframes_t position,
		   jack_nframes_t initial_length,
		   AnchorPoint);

	/* constructor for xfade between two regions that are overlapped in any way
	   except the "internal" case.
	*/
	
	Crossfade (ARDOUR::AudioRegion& in, ARDOUR::AudioRegion& out, CrossfadeModel, bool active);

	/* the usual XML constructor */

	Crossfade (const ARDOUR::Playlist&, XMLNode&);
	virtual ~Crossfade();

	bool operator== (const ARDOUR::Crossfade&);

	XMLNode& get_state (void);
	int set_state (const XMLNode&);

	ARDOUR::AudioRegion& in() const { return *_in; }
	ARDOUR::AudioRegion& out() const { return *_out; }
	
	jack_nframes_t read_at (Sample *buf, Sample *mixdown_buffer, 
				float *gain_buffer, jack_nframes_t position, jack_nframes_t cnt, 
				uint32_t chan_n,
				jack_nframes_t read_frames = 0,
				jack_nframes_t skip_frames = 0);
	
	bool refresh ();

	uint32_t upper_layer () const {
		return std::max (_in->layer(), _out->layer());
	}

	uint32_t lower_layer () const {
		return std::min (_in->layer(), _out->layer());
	}

	bool involves (ARDOUR::AudioRegion& region) const {
		return _in == &region || _out == &region;
	}

	bool involves (ARDOUR::AudioRegion& a, ARDOUR::AudioRegion& b) const {
		return (_in == &a && _out == &b) || (_in == &b && _out == &a);
	}

	jack_nframes_t length() const { return _length; }
	jack_nframes_t overlap_length() const;
	jack_nframes_t position() const { return _position; }

	sigc::signal<void,Crossfade*> Invalidated;
	sigc::signal<void>            GoingAway;

	bool covers (jack_nframes_t frame) const {
		return _position <= frame && frame < _position + _length;
	}

	OverlapType coverage (jack_nframes_t start, jack_nframes_t end) const;

	UndoAction get_memento() const;	

	static void set_buffer_size (jack_nframes_t);

	bool active () const { return _active; }
	void set_active (bool yn);

	bool following_overlap() const { return _follow_overlap; }
	bool can_follow_overlap() const;
	void set_follow_overlap (bool yn);

	Curve& fade_in() { return _fade_in; } 
	Curve& fade_out() { return _fade_out; }

	jack_nframes_t set_length (jack_nframes_t);
	
	static jack_nframes_t short_xfade_length() { return _short_xfade_length; }
	static void set_short_xfade_length (jack_nframes_t n);

	static Change ActiveChanged;

  private:
	friend struct CrossfadeComparePtr;

	static jack_nframes_t _short_xfade_length;

	ARDOUR::AudioRegion* _in;
	ARDOUR::AudioRegion* _out;
	bool                 _active;
	bool                 _in_update;
	OverlapType           overlap_type;
	jack_nframes_t       _length;
	jack_nframes_t       _position;
	AnchorPoint          _anchor_point;
	bool                 _follow_overlap;
	bool                 _fixed;
	Curve _fade_in;
	Curve _fade_out;

	static Sample* crossfade_buffer_out;
	static Sample* crossfade_buffer_in;

	void initialize (bool savestate=true);
	int  compute (ARDOUR::AudioRegion&, ARDOUR::AudioRegion&, CrossfadeModel);
	bool update (bool force);

	StateManager::State* state_factory (std::string why) const;
	Change restore_state (StateManager::State&);

	void member_changed (ARDOUR::Change);

};


} // namespace ARDOUR

#endif /* __ardour_overlap_h__ */	
