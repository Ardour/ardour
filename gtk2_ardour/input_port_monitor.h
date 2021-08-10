/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_input_port_monitor_h__
#define __gtk_ardour_input_port_monitor_h__

#include <gtkmm/box.h>
#include <gtkmm/alignment.h>

#include "gtkmm2ext/cairo_widget.h"

#include "ardour/circular_buffer.h"
#include "ardour/session_handle.h"

namespace ArdourWidgets
{
	class FastMeter;
}

class InputPortMonitor : public Gtk::EventBox
{
public:
	enum Orientation {
		Vertical,
		Horizontal
	};

	InputPortMonitor (ARDOUR::DataType, ARDOUR::samplecnt_t, Orientation);
	~InputPortMonitor ();

	void clear ();
	void update (float, float);                  // FastMeter
	void update (float const*);                  // EventMeter
	void update (ARDOUR::CircularSampleBuffer&); // InputScope
	void update (ARDOUR::CircularEventBuffer&);  // EventMonitor

private:
	class InputScope : public CairoWidget
	{
	public:
		InputScope (ARDOUR::samplecnt_t, int length , int gauge, Orientation);
		void update (ARDOUR::CircularSampleBuffer&);
		void clear ();
		void parameter_changed (std::string const&);

	protected:
		void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
		void on_size_request (Gtk::Requisition*);
		void on_size_allocate (Gtk::Allocation&);

	private:
		void dpi_reset ();

		int                 _pos;
		ARDOUR::samplecnt_t _rate;
		int                 _min_length;
		int                 _min_gauge;
		Orientation         _orientation;
		float               _clip_level;
		bool                _show_clip;
		bool                _logscale;

		Cairo::RefPtr<Cairo::ImageSurface> _surface;
	};

	class EventMeter : public CairoWidget
	{
	public:
		EventMeter (Orientation);
		void update (float const*);
		void clear ();

	protected:
		void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
		void on_size_request (Gtk::Requisition*);

	private:
		void dpi_reset ();

		Glib::RefPtr<Pango::Layout> _layout;
		float                       _chn[17];
		int                         _length;
		int                         _extent;
		Orientation                 _orientation;
	};

	class EventMonitor : public CairoWidget
	{
	public:
		EventMonitor (Orientation);
		void update (ARDOUR::CircularEventBuffer&);
		void clear ();

	protected:
		void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
		void on_size_request (Gtk::Requisition*);

	private:
		void dpi_reset ();

		ARDOUR::CircularEventBuffer::EventList _l;
		Glib::RefPtr<Pango::Layout>            _layout;
		int                                    _width;
		int                                    _height;
		Orientation                            _orientation;
	};

	void setup_audio_meter ();
	void color_handler ();
	void parameter_changed (std::string const&);

	Gtk::Box*                 _box;
	Gtk::Alignment            _bin;
	ARDOUR::DataType          _dt;
	ArdourWidgets::FastMeter* _audio_meter;
	InputScope*               _audio_scope;
	EventMeter*               _midi_meter;
	EventMonitor*             _midi_monitor;
	Orientation               _orientation;
};

#endif
