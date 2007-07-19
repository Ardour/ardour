#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/comboboxtext.h>

#include "ardour_dialog.h"

class MidiPortDialog : public ArdourDialog
{
  public:
	MidiPortDialog ();
	~MidiPortDialog ();

	Gtk::HBox         hpacker;
	Gtk::Label        port_label;
	Gtk::Entry        port_name;
	Gtk::ComboBoxText port_mode_combo;

  private:
	void entry_activated ();
};
