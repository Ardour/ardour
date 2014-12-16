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
#include <cairomm/cairomm.h>
#include <pangomm.h>

#include "ardour/ardour.h"
#include "pbd/signals.h"

#include "canvas/fwd.h"
#include "canvas/types.h"

namespace ARDOUR {
	class TempoSection;
	class MeterSection;
	class Location;
}

namespace ArdourCanvas
{
	class Container;
	class Item;
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
    ARDOUR::Location* location() const { return _location; }

    virtual void canvas_height_set (double);
    void set_has_scene_change (bool);
    bool can_have_scene_change () const { return _location && _location->scene_change (); }
	bool has_scene_change () const { return _have_scene_change; }


	void set_name (const std::string&);

        void set_selected (bool yn);
        void set_color (ArdourCanvas::Color);
        void reset_color ();
        
	void set_position (framepos_t start, framepos_t end = -1) {
                return _set_position (start, end);
        }

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
    ArdourCanvas::Text* _marker_lock_text;

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
        
        virtual void flags_changed ();
        virtual void bounds_changed ();
        virtual void name_changed ();
        virtual void scene_change_active_changed ();
        virtual void scene_change_changed ();
        PBD::ScopedConnection scene_change_active_connection;
        
        virtual void _set_position (framepos_t, framepos_t);

        void pick_basic_color (ArdourCanvas::Color);
	virtual void use_color ();
	virtual void reposition ();
	virtual void setup_name_display ();
	virtual void setup_line ();
        
        static const double _marker_height;

private:
	/* disallow copy construction */
	Marker (Marker const &);
	Marker & operator= (Marker const &);

        PBD::ScopedConnectionList location_connections;
        void connect_to_scene_change_signals ();
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
	void use_color ();
	void reposition ();
        void setup_line ();
        void canvas_height_set (double);

    protected:
    framepos_t _end_frame;
	ArdourCanvas::Line* _end_line;
	ArdourCanvas::Rectangle* _start_handler;
	ArdourCanvas::Rectangle* _end_handler;
    Cairo::RefPtr<Cairo::Surface> _pattern;
    PBD::ScopedConnection parameter_connection;

    void bounds_changed ();
    void _set_position (framepos_t, framepos_t);
    void parameter_changed (const std::string&);
};

/** A variant on RangeMarker that is used to draw markers/locations on top of the ruler using
    a washout gradient. It differs from RangeMarker only in the coloration.
 */
class RulerMarker: public RangeMarker 
{
    public:
        RulerMarker (ARDOUR::Location*, PublicEditor& editor, ArdourCanvas::Container &, double height, guint32 rgba, const std::string& text,
                     framepos_t start, framepos_t end);
        
	void use_color ();
        void setup_name_display ();
};

class TempoMarker : public Marker
{
  public:
        TempoMarker (PublicEditor& editor, ArdourCanvas::Container &, double height, guint32 rgba, const std::string& text, ARDOUR::TempoSection&);
	~TempoMarker ();

	ARDOUR::TempoSection& tempo() const { return _tempo; }

  private:
	ARDOUR::TempoSection& _tempo;
};

class MeterMarker : public Marker
{
  public:
        MeterMarker (PublicEditor& editor, ArdourCanvas::Container &, double height, guint32 rgba, const std::string& text, ARDOUR::MeterSection&);
	~MeterMarker ();

	ARDOUR::MeterSection& meter() const { return _meter; }

  private:
	ARDOUR::MeterSection& _meter;
};

#endif /* __gtk_ardour_marker_h__ */
