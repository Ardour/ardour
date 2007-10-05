#include <string>
#include <sigc++/bind.h>
#include <gtkmm/stock.h>

#include <pbd/convert.h>
#include <gtkmm2ext/utils.h>

#include "midi_port_dialog.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;

static const char* mode_strings[] = { "duplex", "output", "input",  (char*) 0 };

MidiPortDialog::MidiPortDialog ()
	: ArdourDialog ("Add MIDI port"),
	  port_label (_("Port name"))
	
{
	vector<string> str = internationalize (PACKAGE, mode_strings);
	set_popdown_strings (port_mode_combo, str);
	port_mode_combo.set_active_text (str.front());

	hpacker.pack_start (port_label);
	hpacker.pack_start (port_name);
	hpacker.pack_start (port_mode_combo);

	port_label.show ();
	port_name.show ();
	port_mode_combo.show ();
	hpacker.show ();

	get_vbox()->pack_start (hpacker);

	port_name.signal_activate().connect (mem_fun (*this, &MidiPortDialog::entry_activated));

	add_button (Stock::ADD, RESPONSE_ACCEPT);
	add_button (Stock::CANCEL, RESPONSE_CANCEL);
}

void
MidiPortDialog::entry_activated ()
{
	response (RESPONSE_ACCEPT);
}

MidiPortDialog::~MidiPortDialog ()
{

}
