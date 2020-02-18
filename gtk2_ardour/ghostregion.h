/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_gtk_ghost_region_h__
#define __ardour_gtk_ghost_region_h__

#include <vector>
#include <boost/unordered_map.hpp>
#include "pbd/signals.h"

namespace ArdourWaveView {
	class WaveView;
}

class NoteBase;
class Note;
class Hit;
class MidiStreamView;
class TimeAxisView;
class RegionView;
class MidiRegionView;

class GhostRegion : public sigc::trackable
{
public:
	GhostRegion(RegionView& rv,
	            ArdourCanvas::Container* parent,
	            TimeAxisView& tv,
	            TimeAxisView& source_tv,
	            double initial_unit_pos);

	virtual ~GhostRegion();

	virtual void set_samples_per_pixel (double) = 0;
	virtual void set_height();
	virtual void set_colors();

	void set_duration(double units);

	guint source_track_color(unsigned char alpha = 0xff);
	bool is_automation_ghost();

	RegionView& parent_rv;
	/** TimeAxisView that is the AutomationTimeAxisView that we are on */
	TimeAxisView& trackview;
	/** TimeAxisView that we are a ghost for */
	TimeAxisView& source_trackview;
	ArdourCanvas::Container* group;
	ArdourCanvas::Rectangle* base_rect;
};

class AudioGhostRegion : public GhostRegion {
public:
	AudioGhostRegion(RegionView& rv,
	                 TimeAxisView& tv,
	                 TimeAxisView& source_tv,
	                 double initial_unit_pos);

	void set_samples_per_pixel (double);
	void set_height();
	void set_colors();

	std::vector<ArdourWaveView::WaveView*> waves;
};

class MidiGhostRegion : public GhostRegion {
public:
	class GhostEvent : public sigc::trackable
	{
	public:
		GhostEvent(::NoteBase *, ArdourCanvas::Container *);
		virtual ~GhostEvent ();

		NoteBase* event;
		ArdourCanvas::Item* item;
		bool is_hit;
	};

	MidiGhostRegion(MidiRegionView& rv,
	                TimeAxisView& tv,
	                TimeAxisView& source_tv,
	                double initial_unit_pos);

	MidiGhostRegion(MidiRegionView& rv,
	                MidiStreamView& msv,
	                TimeAxisView& source_tv,
	                double initial_unit_pos);

	~MidiGhostRegion();

	MidiStreamView* midi_view();

	void set_height();
	void set_samples_per_pixel (double spu);
	void set_colors();

	void update_contents_height();

	void add_note(NoteBase*);
	void update_note (GhostEvent* note);
	void update_hit (GhostEvent* hit);
	void remove_note (NoteBase*);

	void redisplay_model();
	void clear_events();

private:
	ArdourCanvas::Container* _note_group;
	Gtkmm2ext::Color _outline;
	ArdourCanvas::Rectangle* _tmp_rect;
	ArdourCanvas::Polygon* _tmp_poly;

	MidiRegionView& parent_mrv;
	typedef Evoral::Note<Temporal::Beats> NoteType;
	MidiGhostRegion::GhostEvent* find_event (boost::shared_ptr<NoteType>);

	typedef boost::unordered_map<boost::shared_ptr<NoteType>, MidiGhostRegion::GhostEvent* > EventList;
	EventList events;
	EventList::iterator _optimization_iterator;
};

#endif /* __ardour_gtk_ghost_region_h__ */
