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

class Marker : public sigc::trackable
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
                Range,
	};


	Marker (ARDOUR::Location*, PublicEditor& editor, ArdourCanvas::Container &, double height, guint32 rgba, const std::string& text, Type,
		framepos_t frame = 0, bool handle_events = true);

	virtual ~Marker ();

	static PBD::Signal1<void,Marker*> CatchDeletion;

	ArdourCanvas::Item& the_item() const;

        void set_has_scene_change (bool);

	void set_name (const std::string&);

	virtual void set_position (framepos_t);
	virtual void set_color_rgba (uint32_t rgba);

	framepos_t position() const { return frame_position; }

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

        static double marker_height() { return _marker_height; }

        static const char * default_new_marker_prefix;

  protected:
	PublicEditor& editor;
        ARDOUR::Location* _location;

	Pango::FontDescription name_font;

	ArdourCanvas::Container* _parent;
	ArdourCanvas::Container *group;
	ArdourCanvas::Polygon *mark;
        ArdourCanvas::Text *_name_item;
	ArdourCanvas::Line* _start_line;
	ArdourCanvas::Rectangle* _name_background;
	ArdourCanvas::Rectangle* _scene_change_rect;
	ArdourCanvas::Text* _scene_change_text;

	std::string  _name;
	double        unit_position;
	framepos_t    frame_position;
	double       _shift;
	Type         _type;
	int           name_height;
	bool         _shown;
	double       _height;
	uint32_t     _color;
	double       _left_label_limit; ///< the number of pixels available to the left of this marker for a label
	double       _right_label_limit; ///< the number of pixels available to the right of this marker for a label
	double       _label_offset;
        bool         _have_scene_change;

	void reposition ();
	virtual void setup_name_display ();
	virtual void setup_line ();
        
        static const double _marker_height;

private:
	/* disallow copy construction */
	Marker (Marker const &);
	Marker & operator= (Marker const &);
};

/** A Marker that displays a range (start+end) rather than a single location
 */
class RangeMarker : public Marker
{
    public:
        RangeMarker (ARDOUR::Location*, PublicEditor& editor, ArdourCanvas::Container &, double height, guint32 rgba, const std::string& text,
                     framepos_t start, framepos_t end);
        ~RangeMarker ();
        
	void setup_name_display ();
	void set_color_rgba (uint32_t rgba);
        void set_position (framepos_t);
        void setup_line ();

    protected:
        framepos_t _end_frame;
	ArdourCanvas::Line* _end_line;
        Cairo::RefPtr<Cairo::Surface> _pattern;
};

/** A variant on RangeMarker that is used to draw markers/locations on top of the ruler using
    a washout gradient. It differs from RangeMarker only in the coloration.
 */
class RulerMarker: public RangeMarker 
{
    public:
        RulerMarker (ARDOUR::Location*, PublicEditor& editor, ArdourCanvas::Container &, double height, guint32 rgba, const std::string& text,
                     framepos_t start, framepos_t end);
        
	void set_color_rgba (uint32_t rgba);
};

class TempoMarker : public Marker
{
  public:
        TempoMarker (PublicEditor& editor, ArdourCanvas::Container &, guint32 rgba, const std::string& text, ARDOUR::TempoSection&);
	~TempoMarker ();

	ARDOUR::TempoSection& tempo() const { return _tempo; }

  private:
	ARDOUR::TempoSection& _tempo;
};

class MeterMarker : public Marker
{
  public:
        MeterMarker (PublicEditor& editor, ArdourCanvas::Container &, guint32 rgba, const std::string& text, ARDOUR::MeterSection&);
	~MeterMarker ();

	ARDOUR::MeterSection& meter() const { return _meter; }

  private:
	ARDOUR::MeterSection& _meter;
};

#endif /* __gtk_ardour_marker_h__ */
