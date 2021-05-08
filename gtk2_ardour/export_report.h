/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_export_report_h_
#define _gtkardour_export_report_h_

#include <cairo/cairo.h>
#include <gtkmm/notebook.h>
#include <gtkmm/togglebutton.h>

#include "gtkmm2ext/cairo_widget.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour/export_status.h"

#include "ardour_dialog.h"

class CimgArea : public CairoWidget
{
public:
	CimgArea (Cairo::RefPtr<Cairo::ImageSurface> sf)
		: CairoWidget()
		, _surface(sf)
	{
		set_size_request (sf->get_width (), sf->get_height ());
	}

protected:
	virtual void background (cairo_t* cr, cairo_rectangle_t* r) {
		cairo_set_source_surface (cr, _surface->cobj(), 0, 0);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_paint (cr);
	}

	virtual void overlay (cairo_t* cr, cairo_rectangle_t* r) {}

	virtual void render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t* r)
	{
		ctx->rectangle (r->x, r->y, r->width, r->height);
		ctx->clip ();
		background (ctx->cobj(), r);
		overlay (ctx->cobj(), r);
	}

	Cairo::RefPtr<Cairo::ImageSurface> _surface;
};

class CimgPlayheadArea : public CimgArea
{
public:
	CimgPlayheadArea (Cairo::RefPtr<Cairo::ImageSurface> sf, float x0, float w, bool h = false)
	: CimgArea (sf)
	, _playhead(-1)
	, _x0 (x0)
	, _aw (w)
	, _highlight (h)
	{
	}

	void set_playhead (float pos) {
		if (rint (_playhead * _aw) == rint (pos * _aw)) {
			return;
		}
		if (_playhead == -1 || pos == -1) {
			set_dirty ();
		} else {
			invalidate (_playhead);
			invalidate (pos);
		}
		_playhead = pos;
	}

	sigc::signal<void, float> seek_playhead;

protected:

	virtual void overlay (cairo_t* cr, cairo_rectangle_t* r) {
		if (_playhead > 0 && _playhead < 1.0 && _aw > 0) {
			if (_highlight) {
				cairo_rectangle (cr, _x0, 0, _aw, _surface->get_height());
				cairo_set_source_rgba (cr, .4, .4, .6, .4);
				cairo_fill (cr);
			}

			const float x = _playhead * _aw;
			const float h = _surface->get_height();
			cairo_set_source_rgba (cr, 1, 0, 0, 1);
			cairo_set_line_width (cr, 1.5);
			cairo_move_to (cr, _x0 + x, 0);
			cairo_line_to (cr, _x0 + x, h);
			cairo_stroke (cr);
		}
	}

	bool on_button_press_event (GdkEventButton *ev) {
		CairoWidget::on_button_press_event (ev);
		if (ev->button == 1 && _aw > 0 && ev->x >= _x0 && ev->x <= _x0 + _aw) {
			seek_playhead (((float) ev->x - _x0) / (float)_aw);
		}
		return true;
	}

private:
	float _playhead;
	float _x0, _aw;
	bool _highlight;

	void invalidate (float pos) {
		if (pos < 0 || pos > 1) { return; }
		const float x = pos * _aw;
		cairo_rectangle_t r;
		r.y = 0;
		r.x = _x0 + x - 1;
		r.width = 3;
		r.height = _surface->get_height();
		set_dirty (&r);
	}
};

class CimgWaveArea : public CimgPlayheadArea
{
public:
	CimgWaveArea (
			Cairo::RefPtr<Cairo::ImageSurface> sf,
			Cairo::RefPtr<Cairo::ImageSurface> sf_log,
			Cairo::RefPtr<Cairo::ImageSurface> sf_rect,
			Cairo::RefPtr<Cairo::ImageSurface> sf_logrec,
			float x0, float w)
	: CimgPlayheadArea (sf, x0, w)
	, _sf_log (sf_log)
	, _sf_rect (sf_rect)
	, _sf_logrec (sf_logrec)
	, _logscale (false)
	, _rectified (false)
	{
	}

	void set_logscale (bool en) {
		_logscale = en;
		set_dirty ();
	}

	void set_rectified (bool en) {
		_rectified = en;
		set_dirty ();
	}

protected:

	virtual void background (cairo_t* cr, cairo_rectangle_t* r) {
		if (_logscale && _rectified) {
			cairo_set_source_surface (cr, _sf_logrec->cobj(), 0, 0);
		} else if (_logscale) {
			cairo_set_source_surface (cr, _sf_log->cobj(), 0, 0);
		} else if (_rectified) {
			cairo_set_source_surface (cr, _sf_rect->cobj(), 0, 0);
		} else {
			cairo_set_source_surface (cr, _surface->cobj(), 0, 0);
		}
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_paint (cr);
	}

private:
	Cairo::RefPtr<Cairo::ImageSurface> _sf_log;
	Cairo::RefPtr<Cairo::ImageSurface> _sf_rect;
	Cairo::RefPtr<Cairo::ImageSurface> _sf_logrec;
	bool _logscale;
	bool _rectified;
};

class ExportReport : public ArdourDialog
{
public:
	typedef boost::shared_ptr<ARDOUR::ExportStatus> StatusPtr;
	ExportReport (ARDOUR::Session*, StatusPtr);
	ExportReport (const std::string & title, const ARDOUR::AnalysisResults & ar);
	int run ();

	void on_response (int response_id) {
		Gtk::Dialog::on_response (response_id);
	}

private:
	void init (const ARDOUR::AnalysisResults &, bool);

	void open_folder (std::string);
	void audition (std::string, unsigned int, int);
	void stop_audition ();
	void play_audition ();
	void audition_active (bool);
	void audition_seek (int, float);
	void audition_progress (ARDOUR::samplecnt_t, ARDOUR::samplecnt_t);
	void on_switch_page (GtkNotebookPage*, guint page_num);
	void on_logscale_toggled (Gtk::ToggleButton*);
	void on_rectivied_toggled (Gtk::ToggleButton*);

	Gtk::Notebook    pages;
	ARDOUR::Session* _session;
	Gtk::Button*     stop_btn;
	Gtk::Button*     play_btn;
	PBD::ScopedConnectionList auditioner_connections;

	struct AuditionInfo {
		AuditionInfo (std::string p, unsigned int c) : path (p), channels (c) {}
		AuditionInfo () : channels (0) {}
		std::string  path;
		unsigned int channels;
	};

	std::map<int, std::list<CimgPlayheadArea*> > timeline;
	std::map<int, AuditionInfo> files;
	std::list<CimgWaveArea*> waves;

	int _audition_num;
	int _page_num;
};

#endif
