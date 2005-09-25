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

    $Id$
*/

#ifndef __gtk_ardour_marker_h__
#define __gtk_ardour_marker_h__

#include <string>
#include <glib.h>
#include <ardour/ardour.h>
#include <gtk-canvas.h>
#include <sigc++/signal_system.h>

namespace ARDOUR {
	class TempoSection;
	class MeterSection;
}

class PublicEditor;

class Marker : public SigC::Object
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

	Marker (PublicEditor& editor, GtkCanvasGroup *parent, guint32 rgba, const string& text, Type, 
		gint (*callback)(GtkCanvasItem *, GdkEvent *, gpointer), jack_nframes_t frame = 0);
	virtual ~Marker ();

	void set_position (jack_nframes_t);
	void set_name (const string&);
	void set_color_rgba (uint32_t rgba);

	void hide ();
	void show ();

	Type type () { return _type; }
	
  protected:
	PublicEditor& editor;

	GtkCanvasItem *group;
	GtkCanvasItem *mark;
	GtkCanvasItem *text;
	GtkCanvasPoints *points;

	double    unit_position;
	jack_nframes_t frame_position;
	unsigned char      shift; /* should be double, but its always small and integral */
	Type      _type;
	
	void reposition ();
};

class TempoMarker : public Marker
{
  public:
	TempoMarker (PublicEditor& editor, GtkCanvasGroup *parent, guint32 rgba, const string& text, ARDOUR::TempoSection&, 
		     gint (*callback)(GtkCanvasItem *, GdkEvent *, gpointer));
	~TempoMarker ();

	ARDOUR::TempoSection& tempo() const { return _tempo; }

  private:
	ARDOUR::TempoSection& _tempo;
};

class MeterMarker : public Marker
{
  public:
	MeterMarker (PublicEditor& editor, GtkCanvasGroup *parent, guint32 rgba, const string& text, ARDOUR::MeterSection&, 
		     gint (*callback)(GtkCanvasItem *, GdkEvent *, gpointer));
	~MeterMarker ();

	ARDOUR::MeterSection& meter() const { return _meter; }

  private:
	ARDOUR::MeterSection& _meter;
};

#endif /* __gtk_ardour_marker_h__ */
