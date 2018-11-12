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

#include <sigc++/signal.h>

#include "ardour/ardour.h"
#include "pbd/signals.h"

#include "canvas/fwd.h"
#include "canvas/types.h"

namespace ARDOUR {
	class TempoSection;
	class MeterSection;
}

class PublicEditor;

/** Location Marker
 *
 * Editor ruler representation of a location marker or range on the timeline.
 */
class ArdourMarker : public sigc::trackable
{
public:
	enum Type {
		Mark,
		Tempo,
		Meter,
		SessionStart, ///< session start
		SessionEnd,   ///< session end
		RangeStart,
		RangeEnd,
		LoopStart,
		LoopEnd,
		PunchIn,
		PunchOut
	};


	ArdourMarker (PublicEditor& editor, ArdourCanvas::Container &, guint32 rgba, const std::string& text, Type,
	              samplepos_t sample = 0, bool handle_events = true);

	virtual ~ArdourMarker ();

	static PBD::Signal1<void,ArdourMarker*> CatchDeletion;

	static void setup_sizes (const double timebar_height);

	ArdourCanvas::Item& the_item() const;

	void set_selected (bool);
	void set_show_line (bool);
	void canvas_height_set (double);

	void set_position (samplepos_t);
	void set_name (const std::string&);
	void set_points_color (uint32_t rgba);
	void set_color_rgba (uint32_t rgba);
	void setup_line ();

	samplepos_t position() const { return sample_position; }

	ArdourCanvas::Container * get_parent() { return _parent; }
	void reparent (ArdourCanvas::Container & parent);

	void hide ();
	void show ();

	Type type () { return _type; }

	void set_left_label_limit (double);
	void set_right_label_limit (double);

	std::string name () const {
		return _name;
	}

	bool label_on_left () const;

protected:
	PublicEditor& editor;

	Pango::FontDescription name_font;

	ArdourCanvas::Container* _parent;
	ArdourCanvas::Container *group;
	ArdourCanvas::Polygon *mark;
	ArdourCanvas::Text *_name_item;
	ArdourCanvas::Points *points;
	ArdourCanvas::Line* _track_canvas_line;
	ArdourCanvas::Rectangle* _name_background;

	std::string  _name;
	double        unit_position;
	samplepos_t    sample_position;
	double       _shift;
	Type         _type;
	int           name_height;
	bool         _selected;
	bool         _shown;
	bool         _line_shown;
	double       _canvas_height;
	uint32_t     _color;
	uint32_t     _points_color;
	double       _left_label_limit; ///< the number of pixels available to the left of this marker for a label
	double       _right_label_limit; ///< the number of pixels available to the right of this marker for a label
	double       _label_offset;

	void reposition ();
	void setup_line_x ();
	void setup_name_display ();

private:
	/* disallow copy construction */
	ArdourMarker (ArdourMarker const &);
	ArdourMarker & operator= (ArdourMarker const &);
};

class TempoMarker : public ArdourMarker
{
  public:
	TempoMarker (PublicEditor& editor, ArdourCanvas::Container &, guint32 rgba, const std::string& text, ARDOUR::TempoSection&);
	~TempoMarker ();

	ARDOUR::TempoSection& tempo() const { return _tempo; }

	void update_height_mark (const double ratio);
  private:
	ARDOUR::TempoSection& _tempo;
};

class MeterMarker : public ArdourMarker
{
  public:
	MeterMarker (PublicEditor& editor, ArdourCanvas::Container &, guint32 rgba, const std::string& text, ARDOUR::MeterSection&);
	~MeterMarker ();

	ARDOUR::MeterSection& meter() const { return _meter; }

  private:
	ARDOUR::MeterSection& _meter;
};

#endif /* __gtk_ardour_marker_h__ */
