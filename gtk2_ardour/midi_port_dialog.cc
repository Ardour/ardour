#include <string>
#include <sigc++/bind.h>
#include <gtkmm/stock.h>

#include "pbd/convert.h"
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "midi_port_dialog.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;

static const char* mode_strings[] = { "duplex", "output", "input",  (char*) 0 };

MidiPortDialog::MidiPortDialog ()
	: ArdourDialog ("add MIDI port dialog")
	, port_label (_("Port name:"))
{
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);
	set_position (Gtk::WIN_POS_MOUSE);
	set_name (N_("MidiPortDialog"));

	WindowTitle title(Glib::get_application_name());
	title += _("Add MIDI port");
	set_title(title.get_string());

	vector<string> str = internationalize (PACKAGE, mode_strings);
	set_popdown_strings (port_mode_combo, str);
	port_mode_combo.set_active_text (str.front());

	hpacker.set_spacing (6);
	hpacker.set_border_width (5);

	hpacker.pack_start (port_label);
	hpacker.pack_start (port_name);
	hpacker.pack_start (port_mode_combo);

	get_vbox()->pack_start (hpacker);

	port_name.signal_activate().connect (mem_fun (*this, &MidiPortDialog::entry_activated));

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::ADD, RESPONSE_ACCEPT);

	show_all_children ();
}

void
MidiPortDialog::entry_activated ()
{
	response (RESPONSE_ACCEPT);
}

MidiPortDialog::~MidiPortDialog ()
{

}
