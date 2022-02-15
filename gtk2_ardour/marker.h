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
#include "canvas/circle.h"

namespace Temporal {
	class Point;
	class TempoPoint;
	class MeterPoint;
	class MusicTimePoint;
}

class PublicEditor;
class RegionView;
class TempoCurve;

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
		BBTPosition,
		SessionStart, ///< session start
		SessionEnd,   ///< session end
		RangeStart,
		RangeEnd,
		LoopStart,
		LoopEnd,
		PunchIn,
		PunchOut,
		RegionCue,
		Cue
	};


	ArdourMarker (PublicEditor& editor, ArdourCanvas::Item &, guint32 rgba, const std::string& text, Type,
	              Temporal::timepos_t const & position, bool handle_events = true, RegionView* rv = 0);

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

	ArdourCanvas::Item * get_parent() { return _parent; }
	void reparent (ArdourCanvas::Item & parent);

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

	/* this will be -1 for all non-cue markers; or cue_index for cue markers */

	void set_cue_index(int c) { _cue_index = c; set_name(_name); }
	int cue_index() const { return _cue_index; }

protected:
	PublicEditor& editor;

	Pango::FontDescription name_font;

	ArdourCanvas::Item* _parent;
	ArdourCanvas::Item *group;
	ArdourCanvas::Circle *_pcue;
	ArdourCanvas::Polygon *_pmark;
	ArdourCanvas::Text *_name_item;
	ArdourCanvas::Points *points;
	ArdourCanvas::Line* _track_canvas_line;
	ArdourCanvas::Rectangle* _name_flag;

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

	int          _cue_index;

	void reposition ();
	void setup_line_x ();
	void setup_name_display ();

private:
	/* disallow copy construction */
	ArdourMarker (ArdourMarker const &);
	ArdourMarker & operator= (ArdourMarker const &);
};

class MetricMarker : public ArdourMarker
{
  public:
	MetricMarker (PublicEditor& ed, ArdourCanvas::Item& parent, guint32 rgba, const std::string& annotation, Type type, Temporal::timepos_t const & pos, bool handle_events);
	virtual Temporal::Point const & point() const = 0;
};

class TempoMarker : public MetricMarker
{
  public:
	TempoMarker (PublicEditor& editor, ArdourCanvas::Item &, guint32 rgba, const std::string& text, Temporal::TempoPoint const &, samplepos_t sample, uint32_t curve_color);
	~TempoMarker ();

	void reset_tempo (Temporal::TempoPoint const & t);

	Temporal::TempoPoint const & tempo() const { return *_tempo; }
	Temporal::Point const & point() const;

	TempoCurve& curve();

  private:
	Temporal::TempoPoint const * _tempo;
	TempoCurve* _curve;
};

class MeterMarker : public MetricMarker
{
  public:
	MeterMarker (PublicEditor& editor, ArdourCanvas::Item &, guint32 rgba, const std::string& text, Temporal::MeterPoint const &);
	~MeterMarker ();

	void reset_meter (Temporal::MeterPoint const & m);

	Temporal::MeterPoint const & meter() const { return *_meter; }
	Temporal::Point const & point() const;

  private:
	Temporal::MeterPoint const * _meter;
};

class BBTMarker : public MetricMarker
{
  public:
	BBTMarker (PublicEditor& editor, ArdourCanvas::Item &, guint32 rgba, const std::string& text, Temporal::MusicTimePoint const &);
	~BBTMarker ();

	void reset_point (Temporal::MusicTimePoint const &);

	Temporal::MusicTimePoint const & mt_point() const { return *_point; }
	Temporal::Point const & point() const;

  private:
	Temporal::MusicTimePoint const * _point;
};

#endif /* __gtk_ardour_marker_h__ */
