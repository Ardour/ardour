/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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
class RegionView;

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
		PunchOut,
		RegionCue
	};


	ArdourMarker (PublicEditor& editor, ArdourCanvas::Container &, guint32 rgba, const std::string& text, Type,
	              ARDOUR::timepos_t const & position, bool handle_events = true, RegionView* rv = 0);
	
	virtual ~ArdourMarker ();

	static PBD::Signal1<void,ArdourMarker*> CatchDeletion;

	static void setup_sizes (const double timebar_height);

	ArdourCanvas::Item& the_item() const;

	void set_selected (bool);
	void set_entered (bool);
	void set_show_line (bool);
	void set_line_height (double);

	void set_position (Temporal::timepos_t const &);
	void set_name (const std::string&);
	void set_points_color (uint32_t rgba);
	void set_color_rgba (uint32_t rgba);
	void setup_line ();

	ARDOUR::timepos_t position() const { return _position; }

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

	/* this will be null for all global markers; non-null for region markers */

	RegionView* region_view() const { return _region_view; }

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
	ARDOUR::timepos_t _position;
	double       _shift;
	Type         _type;
	int           name_height;
	bool         _selected;
	bool         _entered;
	bool         _shown;
	bool         _line_shown;
	uint32_t     _color;
	uint32_t      pre_enter_color;
	uint32_t     _points_color;
	double       _left_label_limit; ///< the number of pixels available to the left of this marker for a label
	double       _right_label_limit; ///< the number of pixels available to the right of this marker for a label
	double       _label_offset;
	double       _line_height;

	RegionView*  _region_view;

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
