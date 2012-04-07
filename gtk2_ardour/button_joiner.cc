#include <iostream>
#include <algorithm>


#include <gtkmm/toggleaction.h>

#include "pbd/compose.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ardour_ui.h"
#include "button_joiner.h"

using namespace Gtk;

ButtonJoiner::ButtonJoiner (const std::string& str, Gtk::Widget& lw, Gtk::Widget& rw)
	: left (lw)
	, right (rw)
	, name (str)
	, active_fill_pattern (0)
	, inactive_fill_pattern (0)
{
	packer.set_homogeneous (true);
	packer.pack_start (left);
	packer.pack_start (right);
	packer.show ();

	align.add (packer);
	align.set (0.5, 1.0);
	align.set_padding (9, 0, 9, 9);
	align.show ();

	add (align);

	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|
		    Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);

	uint32_t border_color;
	uint32_t r, g, b, a;

	border_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: border end", name));
	UINT_TO_RGBA (border_color, &r, &g, &b, &a);
	
	border_r = r/255.0;
	border_g = g/255.0;
	border_b = b/255.0;

	/* child cairo widgets need the color of the inner edge as their
	 * "background"
	 */

	Gdk::Color col;
	col.set_rgb_p (border_r, border_g, border_b);
	provide_background_for_cairo_widget (*this, col);
}

ButtonJoiner::~ButtonJoiner ()
{
	if (active_fill_pattern) {
		cairo_pattern_destroy (active_fill_pattern);
		cairo_pattern_destroy (inactive_fill_pattern);
	}
}

void
ButtonJoiner::render (cairo_t* cr)
{
	double h = get_height();
	
	if (!get_active()) {
		cairo_set_source (cr, inactive_fill_pattern);
	} else {
		cairo_set_source (cr, active_fill_pattern);
	}

	/* outer rect */

	Gtkmm2ext::rounded_top_rectangle (cr, 0, 0, get_width(), h, 8);
	cairo_fill_preserve (cr);

	/* outer edge */

	cairo_set_line_width (cr, 1.5);
	cairo_set_source_rgb (cr, border_r, border_g, border_b);
	cairo_stroke (cr);

	/* inner "edge" */

	Gtkmm2ext::rounded_top_rectangle (cr, 8, 8, get_width() - 16, h - 8, 6);
	cairo_stroke (cr);

}

void
ButtonJoiner::on_size_allocate (Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);
	set_colors ();
}

bool
ButtonJoiner::on_button_release_event (GdkEventButton* ev)
{
	if (_action) {
		_action->activate ();
	}

	return true;
}

void
ButtonJoiner::on_size_request (Gtk::Requisition* r)
{
	CairoWidget::on_size_request (r);
}

void
ButtonJoiner::set_related_action (Glib::RefPtr<Action> act)
{
	Gtkmm2ext::Activatable::set_related_action (act);

	if (_action) {

		action_tooltip_changed ();

		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);
		if (tact) {
			action_toggled ();
			tact->signal_toggled().connect (sigc::mem_fun (*this, &ButtonJoiner::action_toggled));
		} 

		_action->connect_property_changed ("sensitive", sigc::mem_fun (*this, &ButtonJoiner::action_sensitivity_changed));
		_action->connect_property_changed ("visible", sigc::mem_fun (*this, &ButtonJoiner::action_visibility_changed));
		_action->connect_property_changed ("tooltip", sigc::mem_fun (*this, &ButtonJoiner::action_tooltip_changed));
	}
}

void
ButtonJoiner::action_sensitivity_changed ()
{
	if (_action->property_sensitive ()) {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() & ~Gtkmm2ext::Insensitive));
	} else {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() | Gtkmm2ext::Insensitive));
	}
	
}

void
ButtonJoiner::action_visibility_changed ()
{
	if (_action->property_visible ()) {
		show ();
	} else {
		hide ();
	}
}

void
ButtonJoiner::action_tooltip_changed ()
{
	std::string str = _action->property_tooltip().get_value();
	ARDOUR_UI::instance()->set_tip (*this, str);
}

void
ButtonJoiner::action_toggled ()
{
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (_action);

	if (tact) {
		set_active (tact->get_active());
	}
}	

void
ButtonJoiner::set_active_state (Gtkmm2ext::ActiveState s)
{
	bool changed = (_active_state != s);
	CairoWidget::set_active_state (s);
	if (changed) {
		set_colors ();
	}
}

void
ButtonJoiner::set_colors ()
{
	uint32_t start_color;
	uint32_t end_color;
	uint32_t r, g, b, a;

	if (active_fill_pattern) {
		cairo_pattern_destroy (active_fill_pattern);
		cairo_pattern_destroy (inactive_fill_pattern);
	}

	active_fill_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, get_height());
	inactive_fill_pattern = cairo_pattern_create_linear (0.0, 0.0, 0.0, get_height());

	start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill start", name));
	end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill end", name));
	UINT_TO_RGBA (start_color, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgba (inactive_fill_pattern, 0, r/255.0,g/255.0,b/255.0, a/255.0);
	UINT_TO_RGBA (end_color, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgba (inactive_fill_pattern, 1, r/255.0,g/255.0,b/255.0, a/255.0);

	start_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill start active", name));
	end_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: fill end active", name));
	UINT_TO_RGBA (start_color, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgba (active_fill_pattern, 0, r/255.0,g/255.0,b/255.0, a/255.0);
	UINT_TO_RGBA (end_color, &r, &g, &b, &a);
	cairo_pattern_add_color_stop_rgba (active_fill_pattern, 1, r/255.0,g/255.0,b/255.0, a/255.0);

	queue_draw ();
}

