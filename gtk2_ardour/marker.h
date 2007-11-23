/*
    Copyright (C) 2001 Paul Davis 

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

#ifndef __gtk_ardour_marker_h__
#define __gtk_ardour_marker_h__

#include <string>
#include <glib.h>
#include <ardour/ardour.h>
#include <pbd/destructible.h>

#include "canvas.h"

namespace ARDOUR {
	class TempoSection;
	class MeterSection;
}

class PublicEditor;

class Marker : public PBD::Destructible
{
  public:
	enum Type {
		Mark,
		Tempo,
		Meter,
		Start,
		End,
		LoopStart,
		LoopEnd,
		PunchIn,
		PunchOut
	};


	Marker (PublicEditor& editor, ArdourCanvas::Group& parent, guint32 rgba, const string& text, Type, 
		nframes_t frame = 0, bool handle_events = true);

	virtual ~Marker ();

	ArdourCanvas::Item& the_item() const;

	void add_line (ArdourCanvas::Group*, double initial_height);
	void show_line ();
	void hide_line ();

	void set_position (nframes_t);
	void set_name (const string&);
	void set_color_rgba (uint32_t rgba);
	
	nframes64_t position() const { return frame_position; }

	ArdourCanvas::Group * get_parent() { return _parent; }
	void reparent (ArdourCanvas::Group & parent);
	
	void hide ();
	void show ();

	Type type () { return _type; }

  protected:
	PublicEditor& editor;

	ArdourCanvas::Group * _parent;
	ArdourCanvas::Group *group;
	ArdourCanvas::Polygon *mark;
	ArdourCanvas::Text *text;
	ArdourCanvas::Points *points;
	ArdourCanvas::Line *line;
	ArdourCanvas::Points *line_points;

	double        unit_position;
	nframes64_t   frame_position;
	unsigned char shift; /* should be double, but its always small and integral */
	Type         _type;
	
	void reposition ();
};

class TempoMarker : public Marker
{
  public:
        TempoMarker (PublicEditor& editor, ArdourCanvas::Group& parent, guint32 rgba, const string& text, ARDOUR::TempoSection&);
	~TempoMarker ();

	ARDOUR::TempoSection& tempo() const { return _tempo; }

  private:
	ARDOUR::TempoSection& _tempo;
};

class MeterMarker : public Marker
{
  public:
        MeterMarker (PublicEditor& editor, ArdourCanvas::Group& parent, guint32 rgba, const string& text, ARDOUR::MeterSection&);
	~MeterMarker ();

	ARDOUR::MeterSection& meter() const { return _meter; }

  private:
	ARDOUR::MeterSection& _meter;
};

#endif /* __gtk_ardour_marker_h__ */
