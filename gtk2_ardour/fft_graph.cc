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

using namespace std;
using namespace Gtk;
using namespace Gdk;

FFTGraph::FFTGraph(int windowSize)
{
	_logScale = 0;

	_in       = 0;
	_out      = 0;
	_hanning  = 0;
	_logScale = 0;

	_a_window = 0;

	_show_minmax     = false;
	_show_normalized = false;

	setWindowSize(windowSize);
}

void
FFTGraph::setWindowSize(int windowSize)
{
	if (_a_window) {
		Glib::Threads::Mutex::Lock lm  (_a_window->track_list_lock);
		setWindowSize_internal(windowSize);
	} else {
		setWindowSize_internal(windowSize);
	}
}

void
FFTGraph::setWindowSize_internal(int windowSize)
{
	// remove old tracklist & graphs
	if (_a_window) {
		_a_window->clear_tracklist();
	}

	_windowSize = windowSize;
	_dataSize = windowSize / 2;
	if (_in != 0) {
		fftwf_destroy_plan(_plan);
		free(_in);
		_in = 0;
	}

	if (_out != 0) {
		free(_out);
		_out = 0;
	}

	if (_hanning != 0) {
		free(_hanning);
		_hanning = 0;
	}

	if (_logScale != 0) {
		free(_logScale);
		_logScale = 0;
	}

	// When destroying, window size is set to zero to free up memory
	if (windowSize == 0)
		return;

	// FFT input & output buffers
	_in      = (float *) fftwf_malloc(sizeof(float) * _windowSize);
	_out     = (float *) fftwf_malloc(sizeof(float) * _windowSize);

	// Hanning window
	_hanning = (float *) malloc(sizeof(float) * _windowSize);


	// normalize the window
	double sum = 0.0;

	for (int i=0; i < _windowSize; i++) {
		_hanning[i]=0.81f * ( 0.5f - (0.5f * (float) cos(2.0f * M_PI * (float)i / (float)(_windowSize))));
		sum += _hanning[i];
	}

	double isum = 1.0 / sum;

	for (int i=0; i < _windowSize; i++) {
		_hanning[i] *= isum;
	}

	_logScale = (int *) malloc(sizeof(int) * _dataSize);
	//float count = 0;
	for (int i = 0; i < _dataSize; i++) {
		_logScale[i] = 0;
	}
	_plan = fftwf_plan_r2r_1d(_windowSize, _in, _out, FFTW_R2HC, FFTW_ESTIMATE);
}

FFTGraph::~FFTGraph()
{
	// This will free everything
	setWindowSize(0);
}

bool
FFTGraph::on_expose_event (GdkEventExpose* /*event*/)
{
	redraw();
	return true;
}

FFTResult *
FFTGraph::prepareResult(Gdk::Color color, string trackname)
{
	FFTResult *res = new FFTResult(this, color, trackname);

	return res;
}


void
FFTGraph::set_analysis_window(AnalysisWindow *a_window)
{
	_a_window = a_window;
}

void
FFTGraph::draw_scales(Glib::RefPtr<Gdk::Window> window)
{

	Glib::RefPtr<Gtk::Style> style = get_style();
	Glib::RefPtr<Gdk::GC> black = style->get_black_gc();
	Glib::RefPtr<Gdk::GC> white = style->get_white_gc();

	window->draw_rectangle(black, true, 0, 0, width, height);

	/**
	 *  4          5
	 *  _          _
	 *   |        |
	 * 1 |        | 2
	 *   |________|
	 *        3
	 **/

	// Line 1
	window->draw_line(white, h_margin, v_margin, h_margin, height - v_margin );

	// Line 2
	window->draw_line(white, width - h_margin + 1, v_margin, width - h_margin + 1, height - v_margin );

	// Line 3
	window->draw_line(white, h_margin, height - v_margin, width - h_margin, height - v_margin );

#define DB_METRIC_LENGTH 8
	// Line 4
	window->draw_line(white, h_margin - DB_METRIC_LENGTH, v_margin, h_margin, v_margin );

	// Line 5
	window->draw_line(white, width - h_margin + 1, v_margin, width - h_margin + DB_METRIC_LENGTH, v_margin );



	if (graph_gc == 0) {
		graph_gc = GC::create( get_window() );
	}

	Color grey;

	grey.set_rgb_p(0.2, 0.2, 0.2);

	graph_gc->set_rgb_fg_color( grey );

	if (layout == 0) {
		layout = create_pango_layout ("");
		layout->set_font_description (get_style()->get_font());
	}

	// Draw logscale
	int logscale_pos = 0;
	int position_on_scale;


/* TODO, write better scales and change the log function so that octaves are of equal pixel length
	float scale_points[10] = { 55.0, 110.0, 220.0, 440.0, 880.0, 1760.0, 3520.0, 7040.0, 14080.0, 28160.0 };

	for (int x = 0; x < 10; x++) {

		// i = 0.. _dataSize-1
		float freq_at_bin = (SR/2.0) * ((double)i / (double)_dataSize);



			freq_at_pixel = FFT_START * exp( FFT_RANGE * pixel / (double)(currentScaleWidth - 1) );
	}
	*/

	for (int x = 1; x < 8; x++) {
		position_on_scale = (int)floor( (double)currentScaleWidth*(double)x/8.0);

		while (_logScale[logscale_pos] < position_on_scale)
			logscale_pos++;

		int coord = (int)(v_margin + 1.0 + position_on_scale);

		int SR = 44100;

		int rate_at_pos = (int)((double)(SR/2) * (double)logscale_pos / (double)_dataSize);

		char buf[32];
		if (rate_at_pos < 1000)
			snprintf(buf,32,"%dHz",rate_at_pos);
		else
			snprintf(buf,32,"%dk",(int)floor( (float)rate_at_pos/(float)1000) );

		std::string label = buf;

		layout->set_text(label);

		window->draw_line(graph_gc, coord, v_margin, coord, height - v_margin - 1);

		int width, height;
		layout->get_pixel_size (width, height);

		window->draw_layout(white, coord - width / 2, v_margin / 2, layout);

	}

}

void
FFTGraph::redraw()
{
	Glib::Threads::Mutex::Lock lm  (_a_window->track_list_lock);

	draw_scales(get_window());


	if (_a_window == 0)
		return;

	if (!_a_window->track_list_ready)
		return;

	cairo_t *cr;
	cr = gdk_cairo_create(GDK_DRAWABLE(get_window()->gobj()));
	cairo_set_line_width(cr, 1.5);
	cairo_translate(cr, (float)v_margin + 1.0, (float)h_margin);



	// Find "session wide" min & max
	float minf =  1000000000000.0;
	float maxf = -1000000000000.0;

	TreeNodeChildren track_rows = _a_window->track_list.get_model()->children();

	for (TreeIter i = track_rows.begin(); i != track_rows.end(); i++) {

		TreeModel::Row row = *i;
		FFTResult *res = row[_a_window->tlcols.graph];

		// disregard fft analysis from empty signals
		if (res->minimum() == res->maximum()) {
			continue;
		}

		if ( res->minimum() < minf) {
			minf = res->minimum();
		}

		if ( res->maximum() > maxf) {
			maxf = res->maximum();
		}
	}

	if (!_show_normalized) {
		minf = -150.0f;
		maxf = 0.0f;
	}

	//int graph_height = height - 2 * h_margin;



	float fft_pane_size_w = (float)(width  - 2*v_margin) - 1.0;
	float fft_pane_size_h = (float)(height - 2*h_margin);

	double pixels_per_db = (double)fft_pane_size_h / (double)(maxf - minf);

	cairo_rectangle(cr, 0.0, 0.0, fft_pane_size_w, fft_pane_size_h);
	cairo_clip(cr);

	for (TreeIter i = track_rows.begin(); i != track_rows.end(); i++) {

		TreeModel::Row row = *i;

		// don't show graphs for tracks which are deselected
		if (!row[_a_window->tlcols.visible]) {
			continue;
		}

		FFTResult *res = row[_a_window->tlcols.graph];

		// don't show graphs for empty signals
		if (res->minimum() == res->maximum()) {
			continue;
		}

		float mpp;

		if (_show_minmax) {
			mpp = -1000000.0;

			cairo_set_source_rgba(cr, res->get_color().get_red_p(), res->get_color().get_green_p(), res->get_color().get_blue_p(), 0.30);
			cairo_move_to(cr, 0.5f + (float)_logScale[0], 0.5f + (float)( fft_pane_size_h - (int)floor( (res->maxAt(0) - minf) * pixels_per_db) ));

			// Draw the line of maximum values
			for (int x = 1; x < res->length(); x++) {
				if (res->maxAt(x) > mpp)
					mpp = res->maxAt(x);
				mpp = fmax(mpp, minf);
				mpp = fmin(mpp, maxf);

				// If the next point on the log scale is at the same location,
				// don't draw yet
				if (x + 1 < res->length() && _logScale[x] == _logScale[x + 1]) {
					continue;
				}

				float X = 0.5f + (float)_logScale[x];
				float Y = 0.5f + (float)( fft_pane_size_h - (int)floor( (mpp - minf) * pixels_per_db) );

				cairo_line_to(cr, X, Y);

				mpp = -1000000.0;
			}

			mpp = +10000000.0;
			// Draw back to the start using the minimum value
			for (int x = res->length()-1; x >= 0; x--) {
				if (res->minAt(x) < mpp)
					mpp = res->minAt(x);
				mpp = fmax(mpp, minf);
				mpp = fmin(mpp, maxf);

				// If the next point on the log scale is at the same location,
				// don't draw yet
				if (x - 1 > 0 && _logScale[x] == _logScale[x - 1]) {
					continue;
				}

				float X = 0.5f + (float)_logScale[x];
				float Y = 0.5f + (float)( fft_pane_size_h - (int)floor( (mpp - minf) * pixels_per_db) );

				cairo_line_to(cr, X, Y );

				mpp = +10000000.0;
			}

			cairo_close_path(cr);

			cairo_fill(cr);
		}



		// Set color from track
		cairo_set_source_rgb(cr, res->get_color().get_red_p(), res->get_color().get_green_p(), res->get_color().get_blue_p());

		mpp = -1000000.0;

		cairo_move_to(cr, 0.5, fft_pane_size_h-0.5);

		for (int x = 0; x < res->length(); x++) {


			if (res->avgAt(x) > mpp)
				mpp = res->avgAt(x);
			mpp = fmax(mpp, minf);
			mpp = fmin(mpp, maxf);

			// If the next point on the log scale is at the same location,
			// don't draw yet
			if (x + 1 < res->length() && _logScale[x] == _logScale[x + 1]) {
				continue;
			}

			cairo_line_to(cr, 0.5f + (float)_logScale[x], 0.5f + (float)( fft_pane_size_h - (int)floor( (mpp - minf) * pixels_per_db) ));

			mpp = -1000000.0;
		}

		cairo_stroke(cr);
	}

	cairo_destroy(cr);
}

void
FFTGraph::on_size_request(Gtk::Requisition* requisition)
{
	width  = max(requisition->width,  minScaleWidth  + h_margin * 2);
	height = max(requisition->height, minScaleHeight + 2 + v_margin * 2);

	update_size();

	requisition->width  = width;;
	requisition->height = height;
}

void
FFTGraph::on_size_allocate(Gtk::Allocation & alloc)
{
	width = alloc.get_width();
	height = alloc.get_height();

	update_size();

	DrawingArea::on_size_allocate (alloc);
}

void
FFTGraph::update_size()
{
	currentScaleWidth  = width - h_margin*2;
	currentScaleHeight = height - 2 - v_margin*2;

	float SR = 44100;
	float FFT_START = SR/(double)_dataSize;
	float FFT_END = SR/2.0;
	float FFT_RANGE = log( FFT_END / FFT_START);
	float pixel = 0;
	for (int i = 0; i < _dataSize; i++) {
		float freq_at_bin = (SR/2.0) * ((double)i / (double)_dataSize);
		float freq_at_pixel;
		pixel--;
		do {
			pixel++;
			freq_at_pixel = FFT_START * exp( FFT_RANGE * pixel / (double)(currentScaleWidth - 1) );
		} while (freq_at_bin > freq_at_pixel);

		_logScale[i] = (int)floor(pixel);
	}
}

