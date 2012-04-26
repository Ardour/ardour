#ifndef __gtk2_ardour_button_joiner_h__
#define __gtk2_ardour_button_joiner_h__

#include <gtkmm/box.h>
#include <gtkmm/alignment.h>
#include <gtkmm/action.h>

#include "gtkmm2ext/activatable.h"
#include "gtkmm2ext/cairo_widget.h"

class ButtonJoiner : public CairoWidget, public Gtkmm2ext::Activatable {
  public:
	ButtonJoiner (const std::string&, Gtk::Widget&, Gtk::Widget&, bool central_link = false);
	~ButtonJoiner ();

	void set_related_action (Glib::RefPtr<Gtk::Action>);	
	void set_active_state (Gtkmm2ext::ActiveState);

  protected:
	void render (cairo_t*);
	bool on_button_release_event (GdkEventButton*);
	void on_size_request (Gtk::Requisition*);
	void on_size_allocate (Gtk::Allocation&);

	void action_sensitivity_changed ();
	void action_visibility_changed ();
	void action_tooltip_changed ();
	void action_toggled ();

  private:
	Gtk::Widget&   left;
	Gtk::Widget&   right;
	Gtk::HBox      packer;
	Gtk::Alignment align;
	std::string    name;
	cairo_pattern_t* active_fill_pattern;
	cairo_pattern_t* inactive_fill_pattern;
	bool           central_link;
	double         border_r;
	double         border_g;
	double         border_b;
	void set_colors ();
};

#endif /* __gtk2_ardour_button_joiner_h__ */
