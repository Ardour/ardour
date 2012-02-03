#ifndef __gtk2_ardour_button_joiner_h__
#define __gtk2_ardour_button_joiner_h__

#include <gtkmm/box.h>
#include <gtkmm/alignment.h>
#include <gtkmm/action.h>

#include "gtkmm2ext/activatable.h"
#include "gtkmm2ext/cairo_widget.h"

class ButtonJoiner : public CairoWidget, public Gtkmm2ext::Activatable {
  public:
	ButtonJoiner (Gtk::Widget&, Gtk::Widget&);
	void set_related_action (Glib::RefPtr<Gtk::Action>);	
	void set_active_state (Gtkmm2ext::ActiveState);

  protected:
	void render (cairo_t*);
	bool on_button_release_event (GdkEventButton*);
	void on_size_request (Gtk::Requisition*);

	void action_sensitivity_changed ();
	void action_visibility_changed ();
	void action_tooltip_changed ();
	void action_toggled ();

  private:
	Gtk::Widget& left;
	Gtk::Widget& right;
	Gtk::HBox    packer;
	Gtk::Alignment align;

	void set_colors ();
};

#endif /* __gtk2_ardour_button_joiner_h__ */
