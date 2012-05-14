#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/cairo_widget.h"
#include "gtkmm2ext/cairo_packer.h"

void
CairoPacker::draw_background (Gtk::Widget& w, GdkEventExpose*)
{
	int x, y;
	Gtk::Widget* window_parent;
	Glib::RefPtr<Gdk::Window> win = Gtkmm2ext::window_to_draw_on (w, &window_parent);
	
	if (win) {
		
		Cairo::RefPtr<Cairo::Context> context = win->create_cairo_context();
		w.translate_coordinates (*window_parent, 0, 0, x, y);
		context->set_source_rgba (0.149, 0.149, 0.149, 1.0);
		Gtkmm2ext::rounded_rectangle (context, x, y, w.get_allocation().get_width(), w.get_allocation().get_height(), 9);
		context->fill ();
	}
}

CairoHPacker::CairoHPacker ()
{
	Gdk::Color bg;

	bg.set_red (lrint (0.149 * 65535));
	bg.set_green (lrint (0.149 * 65535));
	bg.set_blue (lrint (0.149 * 65535));

	CairoWidget::provide_background_for_cairo_widget (*this, bg);
}

bool
CairoHPacker::on_expose_event (GdkEventExpose* ev)
{
	draw_background (*this, ev);
	return HBox::on_expose_event (ev);
}

CairoVPacker::CairoVPacker ()
{
	Gdk::Color bg;

	bg.set_red (lrint (0.149 * 65535));
	bg.set_green (lrint (0.149 * 65535));
	bg.set_blue (lrint (0.149 * 65535));

	CairoWidget::provide_background_for_cairo_widget (*this, bg);
}

bool
CairoVPacker::on_expose_event (GdkEventExpose* ev)
{
	draw_background (*this, ev);
	return VBox::on_expose_event (ev);
}
