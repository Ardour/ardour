#include <iostream>
#include <algorithm>

#include <gtkmm/toggleaction.h>

#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "button_joiner.h"

using namespace Gtk;

ButtonJoiner::ButtonJoiner (Gtk::Widget& l, Gtk::Widget& r)
	: left (l)
	, right (r)
{
	packer.set_homogeneous (true);
	packer.pack_start (l);
	packer.pack_start (r);
	packer.show ();

	align.add (packer);
	align.set (0.5, 1.0);
	align.set_padding (7, 0, 5, 5);
	align.show ();

	add (align);

	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|
		    Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);
}

void
ButtonJoiner::render (cairo_t* cr)
{
	double h = get_height();
	double r, g, b;
	
	if (_active_state == Gtkmm2ext::ActiveState (0)) {
		r = 0.0;
		g = 0.0;
		b = 0.0;
	} else {
		r = 0.16;
		g = 0.58;
		b = 0.757;
	}

	Gtkmm2ext::rounded_top_rectangle (cr, 0, 0, get_width(), h, 9);
	cairo_set_source_rgb (cr, r, g, b);
	cairo_fill (cr);
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
		if (tact->get_active()) {
			set_active_state (Gtkmm2ext::Active);
		} else {
			unset_active_state ();
		}
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
	double r, g, b;

	if (_active_state == Gtkmm2ext::ActiveState (0)) {
		r = 0.0;
		g = 0.0;
		b = 0.0;
	} else {
		r = 0.16;
		g = 0.58;
		b = 0.757;
	}

	Gdk::Color col;
	col.set_rgb_p (r, g, b);
	provide_background_for_cairo_widget (*this, col);
}

