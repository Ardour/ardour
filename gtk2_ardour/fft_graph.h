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
		
		FFTGraph(int windowSize);
		~FFTGraph();

		void set_analysis_window(AnalysisWindow *a_window);
		
		int windowSize() const { return _windowSize; }
		void setWindowSize(int windowSize);

		void redraw();
		bool on_expose_event (GdkEventExpose* event);
		
		void on_size_request(Gtk::Requisition* requisition);
		void on_size_allocate(Gtk::Allocation & alloc);
		FFTResult *prepareResult(Gdk::Color color, std::string trackname);
		
		const void set_show_minmax     (bool v) { _show_minmax     = v; redraw(); };
		const void set_show_normalized (bool v) { _show_normalized = v; redraw(); };

	private:

		void update_size();

		void setWindowSize_internal(int windowSize);

		void draw_scales(Glib::RefPtr<Gdk::Window> window);
		
		static const int minScaleWidth = 512;
		static const int minScaleHeight = 420;

		int currentScaleWidth;
		int currentScaleHeight;

		static const int h_margin = 20;
		static const int v_margin = 20;
		Glib::RefPtr<Gdk::GC> graph_gc;

		int width;
		int height;
		
		int _windowSize;
		int _dataSize;

		Glib::RefPtr<Pango::Layout> layout;
		AnalysisWindow *_a_window;

		fftwf_plan _plan;

		float *_out;
		float *_in;
		float *_hanning;
		int *_logScale;

		bool _show_minmax;
		bool _show_normalized;

	friend class FFTResult;
};

#endif /* __ardour_fft_graph_h */
