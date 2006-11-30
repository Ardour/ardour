#ifndef __gtkmm2ext_focus_entry_h__
#define __gtkmm2ext_focus_entry_h__

#include <gtkmm/entry.h>

namespace Gtkmm2ext {

class FocusEntry : public Gtk::Entry
{
  public:
	FocusEntry ();
	
  protected:
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
  private:
	bool next_release_selects;
};

}

#endif /* __gtkmm2ext_focus_entry_h__ */
