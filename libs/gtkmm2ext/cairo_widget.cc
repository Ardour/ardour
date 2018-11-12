/*
    Copyright (C) 2009 Paul Davis

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
#if !defined USE_CAIRO_IMAGE_SURFACE && !defined NDEBUG
#define OPTIONAL_CAIRO_IMAGE_SURFACE
#endif

#include "gtkmm2ext/cairo_widget.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#ifdef __APPLE__
#include <gdk/gdk.h>
#include "gtkmm2ext/nsglview.h"
#endif

#include "pbd/i18n.h"

static const char* has_cairo_widget_background_info = "has_cairo_widget_background_info";

bool CairoWidget::_flat_buttons = false;
bool CairoWidget::_boxy_buttons = false;
bool CairoWidget::_widget_prelight = true;

sigc::slot<void,Gtk::Widget*> CairoWidget::focus_handler;

void CairoWidget::set_source_rgb_a( cairo_t* cr, Gdk::Color col, float a)  //ToDo:  this one and the Canvas version should be in a shared file (?)
{
	float r = col.get_red_p ();
	float g = col.get_green_p ();
	float b = col.get_blue_p ();

	cairo_set_source_rgba(cr, r, g, b, a);
}

CairoWidget::CairoWidget ()
	: _active_state (Gtkmm2ext::Off)
	, _visual_state (Gtkmm2ext::NoVisualState)
	, _need_bg (true)
	, _grabbed (false)
	, _name_proxy (this, X_("name"))
	, _current_parent (0)
	, _canvas_widget (false)
	, _nsglview (0)
{
	_name_proxy.connect (sigc::mem_fun (*this, &CairoWidget::on_name_changed));
}

CairoWidget::~CairoWidget ()
{
	if (_canvas_widget) {
		gtk_widget_set_realized (GTK_WIDGET(gobj()), false);
	}
	if (_parent_style_change) _parent_style_change.disconnect();
}

void
CairoWidget::set_canvas_widget ()
{
	assert (!_nsglview);
	assert (!_canvas_widget);
	ensure_style ();
	gtk_widget_set_realized (GTK_WIDGET(gobj()), true);
	_canvas_widget = true;
}

void
CairoWidget::use_nsglview ()
{
	assert (!_nsglview);
	assert (!_canvas_widget);
	assert (!is_realized());
#ifdef ARDOUR_CANVAS_NSVIEW_TAG // patched gdkquartz.h
	_nsglview = Gtkmm2ext::nsglview_create (this);
#endif
}

int
CairoWidget::get_width () const
{
	if (_canvas_widget) {
		return _allocation.get_width ();
	}
	return Gtk::EventBox::get_width ();
}

int
CairoWidget::get_height () const
{
	if (_canvas_widget) {
		return _allocation.get_height ();
	}
	return Gtk::EventBox::get_height ();
}

void
CairoWidget::size_allocate (Gtk::Allocation& alloc)
{
	if (_canvas_widget) {
		memcpy (&_allocation, &alloc, sizeof(Gtk::Allocation));
		return;
	}
	Gtk::EventBox::size_allocate (alloc);
}


bool
CairoWidget::on_button_press_event (GdkEventButton*)
{
	focus_handler (this);
	return false;
}

uint32_t
CairoWidget::background_color ()
{
	if (_need_bg) {
		Gdk::Color bg (get_parent_bg());
		return RGBA_TO_UINT (bg.get_red() / 255, bg.get_green() / 255, bg.get_blue() / 255, 255);
	} else {
		return 0;
	}
}

#ifdef USE_TRACKS_CODE_FEATURES

/* This is Tracks version of this method.

   The use of get_visible_window() in this method is an abuse of the GDK/GTK
   semantics. It can and may break on different GDK backends, and uses a
   side-effect/unintended behaviour in GDK/GTK to try to accomplish something
   which should be done differently. I (Paul) have confirmed this with the GTK
   development team.

   For this reason, this code is not acceptable for ordinary merging into the Ardour libraries.

   Ardour Developers: you are not obligated to maintain the internals of this
   implementation in the face of build-time environment changes (e.g. -D
   defines etc).
*/

bool
CairoWidget::on_expose_event (GdkEventExpose *ev)
{
	cairo_rectangle_t expose_area;
	expose_area.width = ev->area.width;
	expose_area.height = ev->area.height;

#ifdef USE_CAIRO_IMAGE_SURFACE_FOR_CAIRO_WIDGET
	Cairo::RefPtr<Cairo::Context> cr;
	if (get_visible_window ()) {
		expose_area.x = 0;
		expose_area.y = 0;
		if (!_image_surface) {
			_image_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, get_width(), get_height());
		}
		cr = Cairo::Context::create (_image_surface);
	} else {
		expose_area.x = ev->area.x;
		expose_area.y = ev->area.y;
		cr = get_window()->create_cairo_context ();
	}
#else
	expose_area.x = ev->area.x;
	expose_area.y = ev->area.y;
	Cairo::RefPtr<Cairo::Context> cr = get_window()->create_cairo_context ();
#endif

	cr->rectangle (expose_area.x, expose_area.y, expose_area.width, expose_area.height);
	cr->clip ();

	/* paint expose area the color of the parent window bg
	*/

    if (get_visible_window ()) {
        Gdk::Color bg (get_parent_bg());
		cr->rectangle (expose_area.x, expose_area.y, expose_area.width, expose_area.height);
        cr->set_source_rgb (bg.get_red_p(), bg.get_green_p(), bg.get_blue_p());
        cr->fill ();
    }

	render (cr, &expose_area);

#ifdef USE_CAIRO_IMAGE_SURFACE_FOR_CAIRO_WIDGET
	if(get_visible_window ()) {
		_image_surface->flush();
		/* now blit our private surface back to the GDK one */

		Cairo::RefPtr<Cairo::Context> cairo_context = get_window()->create_cairo_context ();

		cairo_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
		cairo_context->clip ();
		cairo_context->set_source (_image_surface, ev->area.x, ev->area.y);
		cairo_context->set_operator (Cairo::OPERATOR_OVER);
		cairo_context->paint ();
	}
#endif

	Gtk::Widget* child = get_child ();

	if (child) {
		propagate_expose (*child, ev);
	}

	return true;
}

#else

/* Ardour mainline: not using Tracks code features.

   Tracks Developers: please do not modify this version of
   ::on_expose_event(). The version used by Tracks is before the preceding
   #else and contains hacks required for the Tracks GUI to work.
*/

bool
CairoWidget::on_expose_event (GdkEventExpose *ev)
{
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_queue_draw (_nsglview, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
		return true;
	}
#endif
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	Cairo::RefPtr<Cairo::Context> cr;
	if (getenv("ARDOUR_IMAGE_SURFACE")) {
		if (!image_surface) {
			image_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, get_width(), get_height());
		}
		cr = Cairo::Context::create (image_surface);
	} else {
		cr = get_window()->create_cairo_context ();
	}
#elif defined USE_CAIRO_IMAGE_SURFACE

	if (!image_surface) {
		image_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, get_width(), get_height());
	}

	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (image_surface);
#else
	Cairo::RefPtr<Cairo::Context> cr = get_window()->create_cairo_context ();
#endif

	cr->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);

	if (_need_bg) {
		cr->clip_preserve ();

		/* paint expose area the color of the parent window bg
		 */

		Gdk::Color bg (get_parent_bg());

		cr->set_source_rgb (bg.get_red_p(), bg.get_green_p(), bg.get_blue_p());
		cr->fill ();
	} else {
		std::cerr << get_name() << " skipped bg fill\n";
		cr->clip ();
	}

	cairo_rectangle_t expose_area;
	expose_area.x = ev->area.x;
	expose_area.y = ev->area.y;
	expose_area.width = ev->area.width;
	expose_area.height = ev->area.height;

	render (cr, &expose_area);

#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	if (getenv("ARDOUR_IMAGE_SURFACE")) {
#endif
#if defined USE_CAIRO_IMAGE_SURFACE || defined OPTIONAL_CAIRO_IMAGE_SURFACE
	image_surface->flush();
	/* now blit our private surface back to the GDK one */

	Cairo::RefPtr<Cairo::Context> cairo_context = get_window()->create_cairo_context ();

	cairo_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_context->clip ();
	cairo_context->set_source (image_surface, 0, 0);
	cairo_context->set_operator (Cairo::OPERATOR_SOURCE);
	cairo_context->paint ();
#endif
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	}
#endif

	return true;
}

#endif

/** Marks the widget as dirty, so that render () will be called on
 *  the next GTK expose event.
 */

void
CairoWidget::set_dirty (cairo_rectangle_t *area)
{
	ENSURE_GUI_THREAD (*this, &CairoWidget::set_dirty);
	if (!area) {
		queue_draw ();
	} else {
		// TODO emit QueueDrawArea -> ArdourCanvas::Widget
		if (QueueDraw ()) {
			return;
		}
		queue_draw_area (area->x, area->y, area->width, area->height);
	}
}

void
CairoWidget::queue_draw ()
{
	if (QueueDraw ()) {
		return;
	}
	Gtk::EventBox::queue_draw ();
}

void
CairoWidget::queue_resize ()
{
	if (QueueResize ()) {
		return;
	}
	Gtk::EventBox::queue_resize ();
}

/** Handle a size allocation.
 *  @param alloc GTK allocation.
 */
void
CairoWidget::on_size_allocate (Gtk::Allocation& alloc)
{
	if (!_canvas_widget) {
		Gtk::EventBox::on_size_allocate (alloc);
	} else {
		memcpy (&_allocation, &alloc, sizeof(Gtk::Allocation));
	}

#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	if (getenv("ARDOUR_IMAGE_SURFACE")) {
#endif
#if defined USE_CAIRO_IMAGE_SURFACE || defined OPTIONAL_CAIRO_IMAGE_SURFACE
	image_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, alloc.get_width(), alloc.get_height());
#endif
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	}
#endif

	if (_canvas_widget) {
		return;
	}
#ifdef __APPLE__
	if (_nsglview) {
		gint xx, yy;
		gtk_widget_translate_coordinates(
				GTK_WIDGET(gobj()),
				GTK_WIDGET(get_toplevel()->gobj()),
				0, 0, &xx, &yy);
		Gtkmm2ext::nsglview_resize (_nsglview, xx, yy, alloc.get_width(), alloc.get_height());
	}
#endif
	set_dirty ();
}

Gdk::Color
CairoWidget::get_parent_bg ()
{
	Widget* parent;

	parent = get_parent ();

	while (parent) {
		void* p = g_object_get_data (G_OBJECT(parent->gobj()), has_cairo_widget_background_info);

		if (p) {
			Glib::RefPtr<Gtk::Style> style = parent->get_style();
			if (_current_parent != parent) {
				if (_parent_style_change) _parent_style_change.disconnect();
				_current_parent = parent;
				_parent_style_change = parent->signal_style_changed().connect (mem_fun (*this, &CairoWidget::on_style_changed));
			}
			return style->get_bg (get_state());
		}

		if (!parent->get_has_window()) {
			parent = parent->get_parent();
		} else {
			break;
		}
	}

	if (parent && parent->get_has_window()) {
		if (_current_parent != parent) {
			if (_parent_style_change) _parent_style_change.disconnect();
			_current_parent = parent;
			_parent_style_change = parent->signal_style_changed().connect (mem_fun (*this, &CairoWidget::on_style_changed));
		}
		return parent->get_style ()->get_bg (parent->get_state());
	}

	return get_style ()->get_bg (get_state());
}

void
CairoWidget::set_active_state (Gtkmm2ext::ActiveState s)
{
	if (_active_state != s) {
		_active_state = s;
		StateChanged ();
	}
}

void
CairoWidget::set_visual_state (Gtkmm2ext::VisualState s)
{
	if (_visual_state != s) {
		_visual_state = s;
		StateChanged ();
	}
}

void
CairoWidget::set_active (bool yn)
{
	/* this is an API simplification for buttons
	   that only use the Active and Normal states.
	*/

	if (yn) {
		set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		unset_active_state ();
	}
}

void
CairoWidget::on_style_changed (const Glib::RefPtr<Gtk::Style>&)
{
	set_dirty ();
}

void
CairoWidget::on_realize ()
{
	Gtk::EventBox::on_realize();
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_overlay (_nsglview, get_window()->gobj());
	}
#endif
}

void
CairoWidget::on_map ()
{
	Gtk::EventBox::on_map();
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_set_visible (_nsglview, true);
		Gtk::Allocation a = get_allocation();
		gint xx, yy;
		gtk_widget_translate_coordinates(
				GTK_WIDGET(gobj()),
				GTK_WIDGET(get_toplevel()->gobj()),
				0, 0, &xx, &yy);
		Gtkmm2ext::nsglview_resize (_nsglview, xx, yy, a.get_width(), a.get_height());
	}
#endif
}

void
CairoWidget::on_unmap ()
{
	Gtk::EventBox::on_unmap();
#ifdef __APPLE__
	if (_nsglview) {
		Gtkmm2ext::nsglview_set_visible (_nsglview, false);
	}
#endif
}

void
CairoWidget::on_state_changed (Gtk::StateType)
{
	/* this will catch GTK-level state changes from calls like
	   ::set_sensitive()
	*/

	if (get_state() == Gtk::STATE_INSENSITIVE) {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() | Gtkmm2ext::Insensitive));
	} else {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() & ~Gtkmm2ext::Insensitive));
	}

	set_dirty ();
}

void
CairoWidget::set_draw_background (bool yn)
{
	_need_bg = yn;
}

void
CairoWidget::provide_background_for_cairo_widget (Gtk::Widget& w, const Gdk::Color& bg)
{
	/* set up @w to be able to provide bg information to
	   any CairoWidgets that are packed inside it.
	*/

	w.modify_bg (Gtk::STATE_NORMAL, bg);
	w.modify_bg (Gtk::STATE_INSENSITIVE, bg);
	w.modify_bg (Gtk::STATE_ACTIVE, bg);
	w.modify_bg (Gtk::STATE_SELECTED, bg);

	g_object_set_data (G_OBJECT(w.gobj()), has_cairo_widget_background_info, (void*) 0xfeedface);
}

void
CairoWidget::set_flat_buttons (bool yn)
{
	_flat_buttons = yn;
}

void
CairoWidget::set_boxy_buttons (bool yn)
{
	_boxy_buttons = yn;
}


void
CairoWidget::set_widget_prelight (bool yn)
{
	_widget_prelight = yn;
}

void
CairoWidget::set_focus_handler (sigc::slot<void,Gtk::Widget*> s)
{
	focus_handler = s;
}
