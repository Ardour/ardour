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
#include <sigc++/signal.h>
#include <libgnomecanvasmm.h>
#include <ardour/configuration.h>
#include "canvas.h"

namespace Gnome {
	namespace Canvas {
		class CanvasNoteEvent;
		class CanvasNote;
		class CanvasHit;
		class Diamond;
	}
}

class MidiStreamView;
class TimeAxisView;

class GhostRegion : public sigc::trackable 
{
public:
	GhostRegion(ArdourCanvas::Group* parent, TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos);
	virtual ~GhostRegion();

	virtual void set_samples_per_unit(double spu) = 0;
	virtual void set_height();
	virtual void set_colors();

	void set_duration(double units);

	guint source_track_color(unsigned char alpha = 0xff);
	bool is_automation_ghost();

	sigc::signal<void,GhostRegion*> GoingAway;

	TimeAxisView& trackview;
	TimeAxisView& source_trackview;
	ArdourCanvas::Group* group;
	ArdourCanvas::SimpleRect* base_rect;
};

class AudioGhostRegion : public GhostRegion {
public:
	AudioGhostRegion(TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos);

	void set_samples_per_unit(double spu);
	void set_height();
	void set_colors();

	std::vector<ArdourCanvas::WaveView*> waves;
};

class MidiGhostRegion : public GhostRegion {
public:
	class Event : public sigc::trackable {
	public:
		Event(ArdourCanvas::CanvasNoteEvent*);
		virtual ~Event() {}

		virtual void x_changed() = 0;
		ArdourCanvas::CanvasNoteEvent* event;
	};

	class Note : public Event {
	public:
		Note(ArdourCanvas::CanvasNote*, ArdourCanvas::Group*);
		~Note();

		void x_changed();
		ArdourCanvas::SimpleRect* rect;
	};

	class Hit : public Event {
	public:
		Hit(ArdourCanvas::CanvasHit*, ArdourCanvas::Group*);
		~Hit();

		void x_changed();
		ArdourCanvas::Diamond* diamond;
	};
	
	MidiGhostRegion(TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos);
	MidiGhostRegion(MidiStreamView& msv, TimeAxisView& source_tv, double initial_unit_pos);
	~MidiGhostRegion();

	MidiStreamView* midi_view();

	void set_height();
	void set_samples_per_unit(double spu);
	void set_colors();

	void update_range();

	void add_note(ArdourCanvas::CanvasNote*);
	void add_hit(ArdourCanvas::CanvasHit*);

	void clear_events();

	typedef std::list<MidiGhostRegion::Event*> EventList;
	EventList events;
};

#endif /* __ardour_gtk_ghost_region_h__ */
