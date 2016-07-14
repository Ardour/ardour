/*
    Copyright (C) 2006 Paul Davis

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

#ifdef COMPILER_MSVC
#include <algorithm>
using std::min; using std::max;
#endif

#include <iostream>

#include <glibmm.h>
#include <glibmm/refptr.h>

#include <gdkmm/gc.h>

#include <gtkmm/widget.h>
#include <gtkmm/style.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treepath.h>

#include "pbd/stl_delete.h"

#include <math.h>

#include "fft_graph.h"
#include "analysis_window.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;

FFTGraph::FFTGraph (int windowSize)
{
	_logScale = 0;

	_in       = 0;
	_out      = 0;
	_hanning  = 0;
	_logScale = 0;

	_a_window = 0;

	_show_minmax       = false;
	_show_normalized   = false;
	_show_proportional = false;

	setWindowSize (windowSize);
}

void
FFTGraph::setWindowSize (int windowSize)
{
	if (_a_window) {
		Glib::Threads::Mutex::Lock lm  (_a_window->track_list_lock);
		setWindowSize_internal (windowSize);
	} else {
		setWindowSize_internal (windowSize);
	}
}

void
FFTGraph::setWindowSize_internal (int windowSize)
{
	// remove old tracklist & graphs
	if (_a_window) {
		_a_window->clear_tracklist ();
	}

	_windowSize = windowSize;
	_dataSize = windowSize / 2;
	if (_in != 0) {
		fftwf_destroy_plan (_plan);
		free (_in);
		_in = 0;
	}

	if (_out != 0) {
		free (_out);
		_out = 0;
	}

	if (_hanning != 0) {
		free (_hanning);
		_hanning = 0;
	}

	if (_logScale != 0) {
		free (_logScale);
		_logScale = 0;
	}

	// When destroying, window size is set to zero to free up memory
	if (windowSize == 0) {
		return;
	}

	// FFT input & output buffers
	_in  = (float *) fftwf_malloc (sizeof (float) * _windowSize);
	_out = (float *) fftwf_malloc (sizeof (float) * _windowSize);

	// Hanning window
	_hanning = (float *) malloc (sizeof (float) * _windowSize);

	// normalize the window
	double sum = 0.0;

	for (unsigned int i = 0; i < _windowSize; ++i) {
		_hanning[i] = 0.5f - (0.5f * (float) cos (2.0f * M_PI * (float)i / (float)(_windowSize)));
		sum += _hanning[i];
	}

	double isum = 2.0 / sum;

	for (unsigned int i = 0; i < _windowSize; i++) {
		_hanning[i] *= isum;
	}

	_logScale = (int *) malloc (sizeof (int) * _dataSize);

	for (unsigned int i = 0; i < _dataSize; i++) {
		_logScale[i] = 0;
	}
	_plan = fftwf_plan_r2r_1d (_windowSize, _in, _out, FFTW_R2HC, FFTW_MEASURE);
}

FFTGraph::~FFTGraph ()
{
	// This will free everything
	setWindowSize (0);
}

bool
FFTGraph::on_expose_event (GdkEventExpose* /*event*/)
{
	redraw ();
	return true;
}

FFTResult *
FFTGraph::prepareResult (Gdk::Color color, string trackname)
{
	FFTResult *res = new FFTResult (this, color, trackname);

	return res;
}


void
FFTGraph::set_analysis_window (AnalysisWindow *a_window)
{
	_a_window = a_window;
}

int
FFTGraph::draw_scales (Glib::RefPtr<Gdk::Window> window)
{
	int label_height = v_margin;

	Glib::RefPtr<Gtk::Style> style = get_style ();
	Glib::RefPtr<Gdk::GC> black = style->get_black_gc ();
	Glib::RefPtr<Gdk::GC> white = style->get_white_gc ();

	window->draw_rectangle (black, true, 0, 0, width, height);

	/**
	 *  4          5
	 *  _          _
	 *   |        |
	 * 1 |        | 2
	 *   |________|
	 *        3
	 **/

	// Line 1
	window->draw_line (white, hl_margin, v_margin, hl_margin, height - v_margin);

	// Line 2
	window->draw_line (white, width - hr_margin + 1, v_margin, width - hr_margin + 1, height - v_margin);

	// Line 3
	window->draw_line (white, hl_margin, height - v_margin, width - hr_margin, height - v_margin);

	// Line 4
	window->draw_line (white, 3, v_margin, hl_margin, v_margin);

	// Line 5
	window->draw_line (white, width - hr_margin + 1, v_margin, width - 3, v_margin);


	if (graph_gc == 0) {
		graph_gc = GC::create (get_window ());
	}

	Color grey;
	grey.set_rgb_p (0.2, 0.2, 0.2);
	graph_gc->set_rgb_fg_color (grey);

	if (layout == 0) {
		layout = create_pango_layout ("");
		layout->set_font_description (get_style ()->get_font ());
	}

	// Draw x-axis scale 1/3 octaves centered around 1K
	int overlap = 0;

	// make sure 1K (x=0) is visible
	for (int x = 0; x < 27; ++x) {
		float freq = powf (2.f, x / 3.0) * 1000.f;
		if (freq <= _fft_start) { continue; }
		if (freq >= _fft_end) { break; }

		const float pos = currentScaleWidth * logf (freq / _fft_start) / _fft_log_base;
		const int coord = floor (hl_margin + pos);

		if (coord < overlap) {
			continue;
		}

		std::stringstream ss;
		if (freq >= 10000) {
			ss <<  std::setprecision (1) << std::fixed << freq / 1000 << "K";
		} else if (freq >= 1000) {
			ss <<  std::setprecision (2) << std::fixed << freq / 1000 << "K";
		} else {
			ss <<  std::setprecision (0) << std::fixed << freq << "Hz";
		}
		layout->set_text (ss.str ());
		int lw, lh;
		layout->get_pixel_size (lw, lh);
		overlap = coord + lw + 3;

		if (coord + lw / 2 > width - hr_margin - 2) {
			break;
		}
		if (v_margin / 2 + lh > label_height) {
			label_height = v_margin / 2 + lh;
		}
		window->draw_line (graph_gc, coord, v_margin, coord, height - v_margin - 1);
		window->draw_layout (white, coord - lw / 2, v_margin / 2, layout);
	}

	// now from 1K down to 4Hz
	for (int x = 0; x > -24; --x) {
		float freq = powf (2.f, x / 3.0) * 1000.f;
		if (freq >= _fft_end) { continue; }
		if (freq <= _fft_start) { break; }

		const float pos = currentScaleWidth * logf (freq / _fft_start) / _fft_log_base;
		const int coord = floor (hl_margin + pos);

		if (x != 0 && coord > overlap) {
			continue;
		}

		std::stringstream ss;
		if (freq >= 10000) {
			ss <<  std::setprecision (1) << std::fixed << freq / 1000 << "K";
		} else if (freq >= 1000) {
			ss <<  std::setprecision (2) << std::fixed << freq / 1000 << "K";
		} else {
			ss <<  std::setprecision (0) << std::fixed << freq << "Hz";
		}
		layout->set_text (ss.str ());
		int lw, lh;
		layout->get_pixel_size (lw, lh);
		overlap = coord - lw - 3;

		if (coord - lw / 2 < hl_margin + 2) {
			break;
		}
		if (x == 0) {
			// just get overlap position
			continue;
		}
		if (v_margin / 2 + lh > label_height) {
			label_height = v_margin / 2 + lh;
		}
		window->draw_line (graph_gc, coord, v_margin, coord, height - v_margin - 1);
		window->draw_layout (white, coord - lw / 2, v_margin / 2, layout);
	}
	return label_height;
}

void
FFTGraph::redraw ()
{
	Glib::Threads::Mutex::Lock lm  (_a_window->track_list_lock);

	int yoff = draw_scales (get_window ());

	if (_a_window == 0)
		return;

	if (!_a_window->track_list_ready)
		return;

	float minf;
	float maxf;

	TreeNodeChildren track_rows = _a_window->track_list.get_model ()->children ();

	if (!_show_normalized) {
		maxf =    0.0f;
		minf = -108.0f;
	} else  {
		minf =  999.0f;
		maxf = -999.0f;
		for (TreeIter i = track_rows.begin (); i != track_rows.end (); i++) {
			TreeModel::Row row = *i;
			FFTResult *res = row[_a_window->tlcols.graph];

			// disregard fft analysis from empty signals
			if (res->minimum (_show_proportional) == res->maximum (_show_proportional)) {
				continue;
			}
			// don't include invisible graphs
			if (!row[_a_window->tlcols.visible]) {
				continue;
			}

			minf = std::min (minf, res->minimum (_show_proportional));
			maxf = std::max (maxf, res->maximum (_show_proportional));
		}
	}

	// clamp range, > -200dBFS, at least 24dB (two y-axis labels) range
	minf = std::max (-200.f, minf);
	if (maxf <= minf) {
		return;
	}

	if (maxf - minf < 24) {
		maxf += 6.f;
		minf = maxf - 24.f;
	}

	cairo_t *cr;
	cr = gdk_cairo_create (GDK_DRAWABLE (get_window ()->gobj ()));
	cairo_set_line_width (cr, 1.5);
	cairo_translate (cr, hl_margin + 1, yoff);

	float fft_pane_size_w = width  - hl_margin - hr_margin;
	float fft_pane_size_h = height - v_margin - 1 - yoff;
	double pixels_per_db = (double)fft_pane_size_h / (double)(maxf - minf);

	// draw y-axis dB
	cairo_set_source_rgba (cr, .8, .8, .8, 1.0);

	int btm_lbl = fft_pane_size_h;
	{
		// y-axis legend
		layout->set_text (_("dBFS"));
		int lw, lh;
		layout->get_pixel_size (lw, lh);
		cairo_move_to (cr, -2 - lw, fft_pane_size_h - lh / 2);
		pango_cairo_update_layout (cr, layout->gobj ());
		pango_cairo_show_layout (cr, layout->gobj ());
		btm_lbl = fft_pane_size_h - lh;
	}

	for (int x = -6; x >= -200; x -= 12) {
		float yp = 1.5 + fft_pane_size_h - rint ((x - minf) * pixels_per_db);

		assert (layout);
		std::stringstream ss;
		ss << x;
		layout->set_text (ss.str ());
		int lw, lh;
		layout->get_pixel_size (lw, lh);

		if (yp + 2 + lh / 2 > btm_lbl) {
			continue;
		}
		if (yp < 2 + lh / 2) {
			continue;
		}

		cairo_set_source_rgba (cr, .8, .8, .8, 1.0);
		cairo_move_to (cr, -2 - lw, yp - lh / 2);
		pango_cairo_update_layout (cr, layout->gobj ());
		pango_cairo_show_layout (cr, layout->gobj ());

		cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
		cairo_move_to (cr, 0, yp);
		cairo_line_to (cr, fft_pane_size_w, yp);
		cairo_stroke (cr);
	}

	cairo_rectangle (cr, 1, 1, fft_pane_size_w, fft_pane_size_h);
	cairo_clip (cr);

	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);

	for (TreeIter i = track_rows.begin (); i != track_rows.end (); i++) {
		TreeModel::Row row = *i;

		// don't show graphs for tracks which are deselected
		if (!row[_a_window->tlcols.visible]) {
			continue;
		}

		FFTResult *res = row[_a_window->tlcols.graph];

		// don't show graphs for empty signals
		if (res->minimum (_show_proportional) == res->maximum (_show_proportional)) {
			continue;
		}

		float mpp;
		float X,Y;

		if (_show_minmax) {

			X = 0.5f + _logScale[0];
			Y = 1.5f + fft_pane_size_h - pixels_per_db * (res->maxAt (0, _show_proportional) - minf);
			cairo_move_to (cr, X, Y);

			// Draw the line of maximum values
			mpp = minf;
			for (unsigned int x = 1; x < res->length () - 1; ++x) {
				mpp = std::max (mpp, res->maxAt (x, _show_proportional));

				if (_logScale[x] == _logScale[x + 1]) {
					continue;
				}

				mpp = fmin (mpp, maxf);
				X = 0.5f + _logScale[x];
				Y = 1.5f + fft_pane_size_h - pixels_per_db * (mpp - minf);
				cairo_line_to (cr, X, Y);
				mpp = minf;
			}

			mpp = maxf;
			// Draw back to the start using the minimum value
			for (int x = res->length () - 1; x >= 0; --x) {
				mpp = std::min (mpp, res->minAt (x, _show_proportional));

				if (_logScale[x] == _logScale[x + 1]) {
					continue;
				}

				mpp = fmax (mpp, minf);
				X = 0.5f + _logScale[x];
				Y = 1.5f + fft_pane_size_h - pixels_per_db * (mpp - minf);
				cairo_line_to (cr, X, Y);
				mpp = maxf;
			}

			cairo_set_source_rgba (cr, res->get_color ().get_red_p (), res->get_color ().get_green_p (), res->get_color ().get_blue_p (), 0.30);
			cairo_close_path (cr);
			cairo_fill (cr);
		}

		// draw max of averages
		X = 0.5f + _logScale[0];
		Y = 1.5f + fft_pane_size_h - pixels_per_db * (res->avgAt (0, _show_proportional) - minf);
		cairo_move_to (cr, X, Y);

		mpp = minf;
		for (unsigned int x = 0; x < res->length () - 1; x++) {
			mpp = std::max (mpp, res->avgAt (x, _show_proportional));

			if (_logScale[x] == _logScale[x + 1]) {
				continue;
			}

			mpp = fmax (mpp, minf);
			mpp = fmin (mpp, maxf);

			X = 0.5f + _logScale[x];
			Y = 1.5f + fft_pane_size_h - pixels_per_db * (mpp - minf);
			cairo_line_to (cr, X, Y);
			mpp = minf;
		}

		cairo_set_source_rgb (cr, res->get_color ().get_red_p (), res->get_color ().get_green_p (), res->get_color ().get_blue_p ());
		cairo_stroke (cr);
	}
	cairo_destroy (cr);
}

void
FFTGraph::on_size_request (Gtk::Requisition* requisition)
{
	width  = max (requisition->width,  minScaleWidth  + hl_margin + hr_margin);
	height = max (requisition->height, minScaleHeight + 2 + v_margin * 2);

	update_size ();

	requisition->width  = width;;
	requisition->height = height;
}

void
FFTGraph::on_size_allocate (Gtk::Allocation & alloc)
{
	width = alloc.get_width ();
	height = alloc.get_height ();

	update_size ();

	DrawingArea::on_size_allocate (alloc);
}

void
FFTGraph::update_size ()
{
	framecnt_t SR = PublicEditor::instance ().session ()->nominal_frame_rate ();
	_fft_start = SR / (double)_dataSize;
	_fft_end = .5 * SR;
	_fft_log_base = logf (.5 * _dataSize);
	currentScaleWidth  = width - hl_margin - hr_margin;
	_logScale[0] = 0;
	for (unsigned int i = 1; i < _dataSize; ++i) {
		_logScale[i] = floor (currentScaleWidth * logf (.5 * i) / _fft_log_base);
	}
}
