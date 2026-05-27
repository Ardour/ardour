/*
 * Copyright (C) 2026 Robin Gareus <robin@gareus.org>
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

#include <ytkmm/ytkmm.h>

#include "gtkmm2ext/cairo_theme.h"
#include "gtkmm2ext/cell_renderer_button.h"
#include "gtkmm2ext/ui_config.h"
#include "gtkmm2ext/utils.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;

CellRendererButton::CellRendererButton ()
	: Glib::ObjectBase (typeid (CellRendererButton))
	, Gtk::CellRenderer ()
	, _property_label (*this, "label", "")
{
	property_mode ()      = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
	property_xpad ()      = 2;
	property_ypad ()      = 2;
	property_sensitive () = true;
}

Glib::PropertyProxy_Base
CellRendererButton::_property_renderable ()
{
	return _property_label.get_proxy ();
}

Glib::PropertyProxy<std::string>
CellRendererButton::property_label ()
{
	return _property_label.get_proxy ();
}

bool
CellRendererButton::activate_vfunc (GdkEvent*, Gtk::Widget&, const Glib::ustring& path, const Gdk::Rectangle&, const Gdk::Rectangle&, Gtk::CellRendererState)
{
	_signal_clicked (path);
	return true;
}

void
CellRendererButton::render_vfunc (const Glib::RefPtr<Gdk::Drawable>& window, Gtk::Widget& widget, const Gdk::Rectangle& /*background_area*/, const Gdk::Rectangle& cell_area, const Gdk::Rectangle& /*expose_area*/, Gtk::CellRendererState flags)
{
	Glib::RefPtr<Pango::Layout>       layout = Pango::Layout::create (widget.get_pango_context ());
	::Cairo::RefPtr<::Cairo::Context> cr     = window->create_cairo_context ();

	cr->rectangle (cell_area.get_x (), cell_area.get_y (), cell_area.get_width (), cell_area.get_height ());
	cr->clip ();
	cr->translate (cell_area.get_x (), cell_area.get_y ());

	const int w = cell_area.get_width ();
	const int h = cell_area.get_height ();

	const float scale = UIConfigurationBase::instance ().get_ui_scale ();
	const bool  boxy  = CairoTheme::boxy_buttons ();

	Gtkmm2ext::Color outline_color = UIConfigurationBase::instance ().color ("generic button: outline");
	Gtkmm2ext::Color fill_color    = UIConfigurationBase::instance ().color ("generic button: fill");
	Gtkmm2ext::Color text_color    = UIConfigurationBase::instance ().color ("gtk_foreground");
	// Gtkmm2ext::Color text_color = get_contrasting_color (UIConfigurationBase::instance().color ("generic button: fill active"));
	// Gdk::Color bg = property_cell_background_gdk ().get_value ();

	float corner_radius = 3.5; // h / 2. - 2 * scale;
	corner_radius       = boxy ? 0 : corner_radius;

	Gtkmm2ext::rounded_rectangle (cr, 0, 0, w, h, corner_radius + 1.5 * scale);
	Gtkmm2ext::set_source_rgba (cr, outline_color);
	cr->fill ();

	int padding = 1 * std::max (1.f, scale);

	Gtkmm2ext::rounded_rectangle (cr, padding, padding, w - 2 * padding, h - 2 * padding, corner_radius);
	//cr->set_source_rgba (bg.get_red_p(), bg.get_green_p(), bg.get_blue_p(), 1.0);
	Gtkmm2ext::set_source_rgba (cr, fill_color);
	cr->fill ();

	/* Text */
	cr->save ();
	cr->rectangle (2, 1, w - 4, h - 2);
	cr->clip ();
	Gtkmm2ext::set_source_rgba (cr, text_color);

	int         tw, th;
	std::string label = _property_label.get_proxy ();
	layout->set_text (label);
	layout->get_pixel_size (tw, th);

	cr->move_to (0.5 * (w - tw), 0.5 * (h - th));
	layout->show_in_cairo_context (cr);
	cr->restore ();

	/* hovering */
	if (UIConfigurationBase::instance ().get_widget_prelight ()) {
		if (flags & 0x02) {
			Gtkmm2ext::rounded_rectangle (cr, padding, padding, w - 2 * padding, h - 2 * padding, corner_radius);
			cr->set_source_rgba (0.905, 0.917, 0.925, 0.2);
			cr->fill ();
		}
	}
}

void
CellRendererButton::get_size_vfunc (Gtk::Widget& widget, const Gdk::Rectangle* /*cell_area*/, int* /*x_offset*/, int* /*y_offset*/, int* width, int* height) const
{
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (widget.get_pango_context ());
	std::string                 label  = _property_label.get_proxy ();
	layout->set_text (label);
	int tw, th;
	layout->get_pixel_size (tw, th);
	*width  = tw + 4;
	*height = th + 2;
}

CellRendererButton::SignalClicked&
CellRendererButton::signal_clicked ()
{
	return _signal_clicked;
}
