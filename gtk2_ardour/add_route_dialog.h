#ifndef __gtk_ardour_add_route_dialog_h__
#define __gtk_ardour_add_route_dialog_h__

#include <string>

#include <gtkmm/entry.h>
#include <gtkmm/dialog.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>

#include <gtkmm2ext/click_box.h>

class AddRouteDialog : public Gtk::Dialog
{
  public:
	AddRouteDialog ();
	~AddRouteDialog ();

	bool track ();
	std::string name_template ();
	int channels ();
	int count ();

  private:
	Gtk::Entry name_template_entry;
	Gtk::RadioButton track_button;
	Gtk::RadioButton bus_button;
	Gtk::Adjustment routes_adjustment;
	Gtk::SpinButton routes_spinner;
	Gtk::ComboBoxText channel_combo;
};

#endif /* __gtk_ardour_add_route_dialog_h__ */
