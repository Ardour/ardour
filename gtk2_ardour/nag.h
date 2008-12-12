#ifndef __gtk_ardour_nag_h__
#define __gtk_ardour_nag_h__

#include "ardour_dialog.h"

#include <gtkmm/label.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/buttonbox.h>

class NagScreen : public ArdourDialog
{
  public:
	~NagScreen();
	
	static NagScreen* maybe_nag (std::string context);
	void nag ();

  private:
	NagScreen (std::string context, bool maybe_subscriber);

	Gtk::Label message;
	Gtk::VButtonBox button_box;
	Gtk::RadioButtonGroup button_group;
	Gtk::RadioButton donate_button;
	Gtk::RadioButton subscribe_button;
	Gtk::RadioButton existing_button;
	Gtk::RadioButton next_time_button;
	Gtk::RadioButton never_again_button;

	void mark_never_again ();
	void mark_subscriber ();
	void mark_affirmed_subscriber ();
	void offer_to_donate ();
	void offer_to_subscribe ();
	bool open_uri (const char*);
	static bool is_subscribed (bool& really);
};

#endif /* __gtk_ardour_nag_h__ */
