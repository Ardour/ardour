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

		Gdk::Color bg = get_bg ();

		context->set_source_rgba (bg.get_red_p(), bg.get_green_p(), bg.get_blue_p(), 1.0);
		Gtkmm2ext::rounded_rectangle (context, x, y, w.get_allocation().get_width(), w.get_allocation().get_height(), 4);
		context->fill ();
	}
}

CairoHPacker::CairoHPacker ()
{
}

void
CairoHPacker::on_realize ()
{
	HBox::on_realize ();
	CairoWidget::provide_background_for_cairo_widget (*this, get_bg ());
}

Gdk::Color
CairoHPacker::get_bg () const
{
	return get_style()->get_bg (Gtk::STATE_NORMAL);
}

bool
CairoHPacker::on_expose_event (GdkEventExpose* ev)
{
	draw_background (*this, ev);
	return HBox::on_expose_event (ev);
}

CairoVPacker::CairoVPacker ()
{
}

bool
CairoVPacker::on_expose_event (GdkEventExpose* ev)
{
	draw_background (*this, ev);
	return VBox::on_expose_event (ev);
}

void
CairoVPacker::on_realize ()
{
	VBox::on_realize ();
	CairoWidget::provide_background_for_cairo_widget (*this, get_bg());
}

Gdk::Color
CairoVPacker::get_bg () const
{
	return get_style()->get_bg (Gtk::STATE_NORMAL);
}
