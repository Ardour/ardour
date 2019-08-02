/*
 * Copyright (C) 2008-2011 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_fft_graph_h
#define __ardour_fft_graph_h

#include "ardour/types.h"
#include <fftw3.h>

#include <gtkmm/drawingarea.h>
#include <gtkmm/treemodel.h>
#include <gdkmm/color.h>

#include <glibmm/refptr.h>

#include <string>

#include "fft_result.h"

class AnalysisWindow;

class FFTGraph : public Gtk::DrawingArea
{
public:

	FFTGraph (int windowSize);
	~FFTGraph ();

	void set_analysis_window (AnalysisWindow *a_window);

	int windowSize () const { return _windowSize; }
	void setWindowSize (int windowSize);

	void redraw ();
	bool on_expose_event (GdkEventExpose* event);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_button_press_event (GdkEventButton*) { return true; }

	void on_size_request (Gtk::Requisition* requisition);
	void on_size_allocate (Gtk::Allocation & alloc);
	FFTResult *prepareResult (Gdk::Color color, std::string trackname);

	void set_show_minmax      (bool v) { _show_minmax       = v; redraw (); }
	void set_show_normalized  (bool v) { _show_normalized   = v; redraw (); }
	void set_show_proportioanl(bool v) { _show_proportional = v; redraw (); }

private:

	void update_size ();

	void setWindowSize_internal (int windowSize);

	int draw_scales (cairo_t*);

	static const int minScaleWidth = 512;
	static const int minScaleHeight = 420;

	static const int hl_margin = 40; // this should scale with font (dBFS labels)
	static const int hr_margin = 12;
	static const int v_margin  = 12;

	int currentScaleWidth;

	int width;
	int height;

	int _yoff;
	int _ann_x;
	int _ann_y;
	cairo_rectangle_t _ann_area;

	unsigned int _windowSize;
	unsigned int _dataSize;

	Glib::RefPtr<Pango::Layout> layout;
	cairo_surface_t* _surface;

	AnalysisWindow *_a_window;

	fftwf_plan _plan;

	float* _out;
	float* _in;
	float* _hanning;
	int*   _logScale;

	bool _show_minmax;
	bool _show_normalized;
	bool _show_proportional;

	float _fft_start;
	float _fft_end;
	float _fft_log_base;

	friend class FFTResult;
};

#endif /* __ardour_fft_graph_h */
