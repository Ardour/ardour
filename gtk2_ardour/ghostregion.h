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

#ifndef __ardour_gtk_ghost_region_h__
#define __ardour_gtk_ghost_region_h__

#include <vector>
#include "pbd/signals.h"

namespace ArdourCanvas {
	class WaveView;
}

class NoteBase;
class Note;
class Hit;
class MidiStreamView;
class TimeAxisView;

class GhostRegion : public sigc::trackable
{
public:
	GhostRegion(ArdourCanvas::Group* parent, TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos);
	virtual ~GhostRegion();

	virtual void set_frames_per_pixel (double) = 0;
	virtual void set_height();
	virtual void set_colors();

	void set_duration(double units);

	guint source_track_color(unsigned char alpha = 0xff);
	bool is_automation_ghost();

	/** TimeAxisView that is the AutomationTimeAxisView that we are on */
	TimeAxisView& trackview;
	/** TimeAxisView that we are a ghost for */
	TimeAxisView& source_trackview;
	ArdourCanvas::Group* group;
	ArdourCanvas::Rectangle* base_rect;

	static PBD::Signal1<void,GhostRegion*> CatchDeletion;
};

class AudioGhostRegion : public GhostRegion {
public:
	AudioGhostRegion(TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos);

	void set_frames_per_pixel (double);
	void set_height();
	void set_colors();

	std::vector<ArdourCanvas::WaveView*> waves;
};

class MidiGhostRegion : public GhostRegion {
public:
	class GhostEvent : public sigc::trackable {
	public:
	    GhostEvent(::NoteBase *, ArdourCanvas::Group *);
	    virtual ~GhostEvent () {}

		NoteBase* event;
		ArdourCanvas::Rectangle* rect;
	};

	MidiGhostRegion(TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos);
	MidiGhostRegion(MidiStreamView& msv, TimeAxisView& source_tv, double initial_unit_pos);
	~MidiGhostRegion();

	MidiStreamView* midi_view();

	void set_height();
	void set_frames_per_pixel (double spu);
	void set_colors();

	void update_range();

	void add_note(Note*);
	void update_note (Note*);
	void remove_note (Note*);

	void clear_events();

private:

	MidiGhostRegion::Event* find_event (Note*);

	typedef std::list<MidiGhostRegion::GhostEvent*> EventList;
	EventList events;
	EventList::iterator _optimization_iterator;
};

#endif /* __ardour_gtk_ghost_region_h__ */
