#include "ardour/session.h"
#include "time_axis_view.h"
#include "streamview.h"
#include "editor_summary.h"
#include "gui_thread.h"
#include "editor.h"
#include "region_view.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;

EditorSummary::EditorSummary (Editor* e)
	: _editor (e),
	  _session (0),
	  _pixmap (0),
	  _regions_dirty (true),
	  _width (512),
	  _height (64),
	  _pixels_per_frame (1)
{
	
}

void
EditorSummary::set_session (Session* s)
{
	_session = s;

	Region::RegionPropertyChanged.connect (sigc::hide (mem_fun (*this, &EditorSummary::set_dirty)));

	_session->RegionRemoved.connect (sigc::hide (mem_fun (*this, &EditorSummary::set_dirty)));
	_session->EndTimeChanged.connect (mem_fun (*this, &EditorSummary::set_dirty));
	_session->StartTimeChanged.connect (mem_fun (*this, &EditorSummary::set_dirty));

	set_dirty ();
}

EditorSummary::~EditorSummary ()
{
	if (_pixmap) {
		gdk_pixmap_unref (_pixmap);
	}
}

bool
EditorSummary::on_expose_event (GdkEventExpose* event)
{
	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	Gdk::Rectangle r = exposure;
	Gdk::Rectangle content (0, 0, _width, _height);
	bool intersects;
	r.intersect (content, intersects);
	
	if (intersects) {

		GdkPixmap* p = get_pixmap (get_window()->gobj ());

		gdk_draw_drawable (
			get_window()->gobj(),
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			p,
 			r.get_x(),
 			r.get_y(),
 			r.get_x(),
 			r.get_y(),
 			r.get_width(),
 			r.get_height()
			);
	}

	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	cairo_set_source_rgb (cr, 0, 1, 0);
	cairo_set_line_width (cr, 2);

	double const s = (_editor->leftmost_position () - _session->current_start_frame ()) * _pixels_per_frame; 
	cairo_move_to (cr, s, 0);
	cairo_line_to (cr, s, _height);
	cairo_stroke (cr);

	double const e = s + _editor->current_page_frames() * _pixels_per_frame;
	cairo_move_to (cr, e, 0);
	cairo_line_to (cr, e, _height);
	cairo_stroke (cr);
	
	cairo_destroy (cr);
	
	return true;
}

GdkPixmap *
EditorSummary::get_pixmap (GdkDrawable* drawable)
{
	if (_regions_dirty) {

		if (_pixmap) {
			gdk_pixmap_unref (_pixmap);
		}
		_pixmap = gdk_pixmap_new (drawable, _width, _height, -1);

		cairo_t* cr = gdk_cairo_create (_pixmap);
		render (cr);
		cairo_destroy (cr);

		_regions_dirty = false;
	}

	return _pixmap;
}

void
EditorSummary::render (cairo_t* cr)
{
	if (_session == 0) {
		return;
	}

	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	int N = 0;
	
	for (PublicEditor::TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		if ((*i)->view()) {
			++N;
		}
	}

	nframes_t const start = _session->current_start_frame ();
	_pixels_per_frame = static_cast<double> (_width) / (_session->current_end_frame() - start);
	double const track_height = static_cast<double> (_height) / N;

	cairo_set_line_width (cr, track_height);

	int n = 0;
	for (PublicEditor::TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		StreamView* s = (*i)->view ();
		if (s) {

			double const v = ((n % 2) == 0) ? 1 : 0.5;
			cairo_set_source_rgb (cr, v, v, v);

			s->foreach_regionview (bind (
						       mem_fun (*this, &EditorSummary::render_region),
						       cr,
						       start,
						       track_height * (n + 0.5)
						       ));
			++n;
		}
	}

}

void
EditorSummary::render_region (RegionView* r, cairo_t* cr, nframes_t start, double y) const
{
	cairo_move_to (cr, (r->region()->position() - start) * _pixels_per_frame, y);
	cairo_line_to (cr, (r->region()->position() - start + r->region()->length()) * _pixels_per_frame, y);
	cairo_stroke (cr);
}

void
EditorSummary::set_dirty ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &EditorSummary::set_dirty));

	_regions_dirty = true;
	queue_draw ();
}

void
EditorSummary::set_bounds_dirty ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &EditorSummary::set_bounds_dirty));
	queue_draw ();
}

void
EditorSummary::on_size_request (Gtk::Requisition *req)
{
	req->width = 64;
	req->height = 64;
}

void
EditorSummary::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);

	_width = alloc.get_width ();
	_height = alloc.get_height ();

	set_dirty ();
}

bool
EditorSummary::on_button_press_event (GdkEventButton* ev)
{
	if (ev->button == 1) {

		nframes_t f = (ev->x / _pixels_per_frame) + _session->current_start_frame();

		nframes_t const h = _editor->current_page_frames () / 2;
		if (f > h) {
			f -= h;
		} else {
			f = 0;
		}
		
		_editor->reset_x_origin (f);
	}

	return true;
}
