#include "ardour/session.h"
#include "time_axis_view.h"
#include "streamview.h"
#include "editor_summary.h"
#include "gui_thread.h"
#include "editor.h"
#include "region_view.h"
#include "rgb_macros.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;

/** Construct an EditorSummary.
 *  @param e Editor to represent.
 */
EditorSummary::EditorSummary (Editor* e)
	: _editor (e),
	  _session (0),
	  _pixmap (0),
	  _regions_dirty (true),
	  _width (512),
	  _height (64),
	  _pixels_per_frame (1),
	  _vertical_scale (1),
	  _dragging (false),
	  _moved (false)
	  
{
	
}

/** Set the session.
 *  @param s Session.
 */
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

/** Destroy */
EditorSummary::~EditorSummary ()
{
	if (_pixmap) {
		gdk_pixmap_unref (_pixmap);
	}
}

/** Handle an expose event.
 *  @param event Event from GTK.
 */
bool
EditorSummary::on_expose_event (GdkEventExpose* event)
{
	/* Render the regions pixmap */
	
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

	/* Render the view rectangle */
	
	pair<double, double> x;
	pair<double, double> y;
	editor_view (&x, &y);
	
	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	cairo_set_source_rgba (cr, 0, 1, 0, 0.25);

	cairo_move_to (cr, x.first, y.first);
	cairo_line_to (cr, x.second, y.first);
	cairo_line_to (cr, x.second, y.second);
	cairo_line_to (cr, x.first, y.second);
	cairo_line_to (cr, x.first, y.first);
	cairo_fill (cr);

	cairo_destroy (cr);
	
	return true;
}

/** @param drawable GDK drawable.
 *  @return pixmap for the regions.
 */
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

/** Render the required regions to a cairo context.
 *  @param cr Context.
 */
void
EditorSummary::render (cairo_t* cr)
{
	if (_session == 0) {
		return;
	}

	/* background */
	
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	/* compute total height of all tracks */
	
	int h = 0;
	for (PublicEditor::TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		h += (*i)->effective_height ();
	}

	nframes_t const start = _session->current_start_frame ();
	_pixels_per_frame = static_cast<double> (_width) / (_session->current_end_frame() - start);
	_vertical_scale = static_cast<double> (_height) / h;

	/* render regions */

	double y = 0;
	for (PublicEditor::TrackViewList::const_iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		StreamView* s = (*i)->view ();

		if (s) {
			double const h = (*i)->effective_height () * _vertical_scale;
			cairo_set_line_width (cr, h);

			s->foreach_regionview (bind (
						       mem_fun (*this, &EditorSummary::render_region),
						       cr,
						       start,
						       y + h / 2
						       ));
			y += h;
		}
	}

}

/** Render a region for the summary.
 *  @param r Region view.
 *  @param cr Cairo context.
 *  @param start Frame offset that the summary starts at.
 *  @param y y coordinate to render at.
 */
void
EditorSummary::render_region (RegionView* r, cairo_t* cr, nframes_t start, double y) const
{
	uint32_t const c = r->get_fill_color ();
	cairo_set_source_rgb (cr, UINT_RGBA_R (c) / 255.0, UINT_RGBA_G (c) / 255.0, UINT_RGBA_B (c) / 255.0);
			
	cairo_move_to (cr, (r->region()->position() - start) * _pixels_per_frame, y);
	cairo_line_to (cr, ((r->region()->position() - start + r->region()->length())) * _pixels_per_frame, y);
	cairo_stroke (cr);
}

/** Set the summary so that the whole thing will be re-rendered next time it is required */
void
EditorSummary::set_dirty ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &EditorSummary::set_dirty));

	_regions_dirty = true;
	queue_draw ();
}

/** Set the summary so that just the view boundary markers will be re-rendered */
void
EditorSummary::set_bounds_dirty ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &EditorSummary::set_bounds_dirty));
	queue_draw ();
}

/** Handle a size request.
 *  @param req GTK requisition
 */
void
EditorSummary::on_size_request (Gtk::Requisition *req)
{
	/* Use a dummy, small width and the actual height that we want */
	req->width = 64;
	req->height = 64;
}

/** Handle a size allocation.
 *  @param alloc GTK allocation.
 */
void
EditorSummary::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);

	_width = alloc.get_width ();
	_height = alloc.get_height ();

	set_dirty ();
}

void
EditorSummary::centre_on_click (GdkEventButton* ev)
{
	nframes_t x = (ev->x / _pixels_per_frame) + _session->current_start_frame();
	nframes_t const xh = _editor->current_page_frames () / 2;
	if (x > xh) {
		x -= xh;
	} else {
		x = 0;
	}
	
	_editor->reset_x_origin (x);
	
	double y = ev->y / _vertical_scale;
	double const yh = _editor->canvas_height () / 2;
	if (y > yh) {
		y -= yh;
	} else {
		y = 0;
	}
	
	_editor->reset_y_origin (y);
}

/** Handle a button press.
 *  @param ev GTK event.
 */
bool
EditorSummary::on_button_press_event (GdkEventButton* ev)
{
	if (ev->button == 1) {

		pair<double, double> xr;
		pair<double, double> yr;
		editor_view (&xr, &yr);

		if (xr.first <= ev->x && ev->x <= xr.second && yr.first <= ev->y && ev->y <= yr.second) {

			/* click inside the view rectangle: drag it */
			_dragging = true;
			_moved = false;
			_x_offset = ev->x - xr.first;
			_y_offset = ev->y - yr.first;
			_editor->_dragging_playhead = true;
			
		} else {
			
			/* click outside the view rectangle: centre the view around the mouse click */
			centre_on_click (ev);
		}
	}

	return true;
}

void
EditorSummary::editor_view (pair<double, double>* x, pair<double, double>* y) const
{
	x->first = (_editor->leftmost_position () - _session->current_start_frame ()) * _pixels_per_frame;
	x->second = x->first + _editor->current_page_frames() * _pixels_per_frame;

	y->first = _editor->get_trackview_group_vertical_offset () * _vertical_scale;
	y->second = y->first + _editor->canvas_height () * _vertical_scale;
}

bool
EditorSummary::on_motion_notify_event (GdkEventMotion* ev)
{
	if (!_dragging) {
		return false;
	}

	_moved = true;

	_editor->reset_x_origin (((ev->x - _x_offset) / _pixels_per_frame) + _session->current_start_frame ());
	_editor->reset_y_origin ((ev->y - _y_offset) / _vertical_scale);

	return true;
}

bool
EditorSummary::on_button_release_event (GdkEventButton* ev)
{
	if (_dragging && !_moved) {
		centre_on_click (ev);
	}
	
	_dragging = false;
	_editor->_dragging_playhead = false;
	return true;
}
