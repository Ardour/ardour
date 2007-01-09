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

#include <iostream>

#include <glibmm.h>
#include <glibmm/refptr.h>

#include <gdkmm/gc.h>

#include <gtkmm/widget.h>
#include <gtkmm/style.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treepath.h>

#include <pbd/stl_delete.h>

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

	setWindowSize(windowSize);
}

void
FFTGraph::setWindowSize(int windowSize)
{
	if (_a_window) {
		Glib::Mutex::Lock lm  (_a_window->track_list_lock);
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
FFTGraph::on_expose_event (GdkEventExpose* event)
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
FFTGraph::analyze(float *window, float *composite)
{	
	int i;
	// Copy the data and apply the hanning window
	for (i = 0; i < _windowSize; i++) {
		_in[i] = window[ i ] * _hanning[ i ];
	}

	fftwf_execute(_plan);

	composite[0] += (_out[0] * _out[0]);
	
	for (i=1; i < _dataSize - 1; i++) { // TODO: check with Jesse whether this is really correct
		composite[i] += (_out[i] * _out[i]) + (_out[_windowSize-i] * _out[_windowSize-i]);
	}
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
	window->draw_line(white, width - h_margin, v_margin, width - h_margin, height - v_margin );

	// Line 3
	window->draw_line(white, h_margin, height - v_margin, width - h_margin, height - v_margin );

#define DB_METRIC_LENGTH 8
	// Line 5
	window->draw_line(white, h_margin - DB_METRIC_LENGTH, v_margin, h_margin, v_margin );
	
	// Line 6
	window->draw_line(white, width - h_margin, v_margin, width - h_margin + DB_METRIC_LENGTH, v_margin );


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
	for (int x = 1; x < 8; x++) {
		position_on_scale = (int)floor( (double)scaleWidth*(double)x/8.0);

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
		
		window->draw_line(graph_gc, coord, v_margin, coord, height - v_margin);

		int width, height;
		layout->get_pixel_size (width, height);
		
		window->draw_layout(white, coord - width / 2, v_margin / 2, layout);
		
	}

}

void
FFTGraph::redraw()
{	
	Glib::Mutex::Lock lm  (_a_window->track_list_lock);

	draw_scales(get_window());
	
	if (_a_window == 0)
		return;

	if (!_a_window->track_list_ready)
		return;
	
	
	// Find "session wide" min & max
	float min =  1000000000000.0;
	float max = -1000000000000.0;
	
	TreeNodeChildren track_rows = _a_window->track_list.get_model()->children();
	
	for (TreeIter i = track_rows.begin(); i != track_rows.end(); i++) {
		
		TreeModel::Row row = *i;
		FFTResult *res = row[_a_window->tlcols.graph];

		// disregard fft analysis from empty signals
		if (res->minimum() == res->maximum()) {
			continue;
		}
		
		if ( res->minimum() < min) {
			min = res->minimum();
		}

		if ( res->maximum() > max) {
			max = res->maximum();
		}
	}
	
	int graph_height = height - 2 * h_margin;

	if (graph_gc == 0) {
		graph_gc = GC::create( get_window() );
	}
	
	
	double pixels_per_db = (double)graph_height / (double)(max - min);
	
	
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
		
		std::string name = row[_a_window->tlcols.trackname];

		// Set color from track
		graph_gc->set_rgb_fg_color( res->get_color() );

		float mpp = -1000000.0;
		int prevx = 0;
		float prevSample = min;
		
		for (int x = 0; x < res->length() - 1; x++) {
			
			if (res->sampleAt(x) > mpp)
				mpp = res->sampleAt(x);
			
			// If the next point on the log scale is at the same location,
			// don't draw yet
			if (x + 1 < res->length() && 
				_logScale[x] == _logScale[x + 1]) {
				continue;
			}

			get_window()->draw_line(
					graph_gc,
					v_margin + 1 + prevx,
					graph_height - (int)floor( (prevSample - min) * pixels_per_db) + h_margin - 1,
					v_margin + 1 + _logScale[x],
					graph_height - (int)floor( (mpp        - min) * pixels_per_db) + h_margin - 1);
			
			prevx = _logScale[x];
			prevSample = mpp;
			

			mpp = -1000000.0;
			
		}
	}

}

void
FFTGraph::on_size_request(Gtk::Requisition* requisition)
{
	width  = scaleWidth  + h_margin * 2;
	height = scaleHeight + 2 + v_margin * 2;

	if (_logScale != 0) {
		free(_logScale);
	}
	_logScale = (int *) malloc(sizeof(int) * _dataSize);

	float SR = 44100;
	float FFT_START = SR/(double)_dataSize;
	float FFT_END = SR/2.0;
	float FFT_RANGE = log( FFT_END / FFT_START);
	float pixel = 0;
	for (int i = 0; i < _dataSize; i++) {
		float freq_at_bin = (SR/2.0) * ((double)i / (double)_dataSize);
		float freq_at_pixel = FFT_START * exp( FFT_RANGE * pixel / (double)scaleWidth );
		while (freq_at_bin > freq_at_pixel) {
			pixel++;
			freq_at_pixel = FFT_START * exp( FFT_RANGE * pixel / (double)scaleWidth );
		}
		_logScale[i] = (int)floor(pixel);
//printf("logscale at %d = %3.3f, freq_at_pixel %3.3f, freq_at_bin %3.3f, scaleWidth %d\n", i, pixel, freq_at_pixel, freq_at_bin, scaleWidth);
	}

	requisition->width  = width;;
	requisition->height = height;
}

void
FFTGraph::on_size_allocate(Gtk::Allocation & alloc)
{
	width = alloc.get_width();
	height = alloc.get_height();
	
	DrawingArea::on_size_allocate (alloc);

}

