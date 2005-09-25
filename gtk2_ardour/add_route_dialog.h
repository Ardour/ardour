#ifndef __gtk_ardour_add_route_dialog_h__
#define __gtk_ardour_add_route_dialog_h__

#include <string>

#include <gtkmm/entry.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/button.h>

#include <gtkmm2ext/click_box.h>

#include "ardour_dialog.h"

class AddRouteDialog : public ArdourDialog
{
  public:
	AddRouteDialog ();
	~AddRouteDialog ();

	bool track ();
	std::string name_template ();
	int channels ();
	int count ();

	Gtk::Button ok_button;
	Gtk::Button cancel_button;

  private:
	Gtk::Entry name_template_entry;
	Gtk::RadioButton track_button;
	Gtk::RadioButton bus_button;
	Gtk::Adjustment routes_adjustment;
	Gtk::SpinButton routes_spinner;
	Gtk::Combo      channel_combo;
};

#endif /* __gtk_ardour_add_route_dialog_h__ */
