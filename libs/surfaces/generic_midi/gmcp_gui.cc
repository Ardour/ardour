#include <iostream>
#include <list>
#include <string>

#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>

#include "gtkmm2ext/utils.h"

#include "generic_midi_control_protocol.h"

#include "i18n.h"

class GMCPGUI : public Gtk::VBox 
{
public:
    GMCPGUI (GenericMidiControlProtocol&);
    ~GMCPGUI ();

private:
    GenericMidiControlProtocol& cp;
    Gtk::ComboBoxText map_combo;
    Gtk::Adjustment bank_adjustment;
    Gtk::SpinButton bank_spinner;

    void binding_changed ();
    void bank_change ();
};

using namespace PBD;
using namespace ARDOUR;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

void*
GenericMidiControlProtocol::get_gui () const
{
	if (!gui) {
		const_cast<GenericMidiControlProtocol*>(this)->build_gui ();
	}
	return gui;
}

void
GenericMidiControlProtocol::tear_down_gui ()
{
	delete (GMCPGUI*) gui;
}

void
GenericMidiControlProtocol::build_gui ()
{
	gui = (void*) new GMCPGUI (*this);
}

/*--------------------*/

GMCPGUI::GMCPGUI (GenericMidiControlProtocol& p)
	: cp (p)
	, bank_adjustment (1, 1, 100, 1, 10)
	, bank_spinner (bank_adjustment)
{
	vector<string> popdowns;
	popdowns.push_back (_("Reset All"));

	for (list<GenericMidiControlProtocol::MapInfo>::iterator x = cp.map_info.begin(); x != cp.map_info.end(); ++x) {
		popdowns.push_back ((*x).name);
	}

	set_popdown_strings (map_combo, popdowns);
	
	if (cp.current_binding().empty()) {
		map_combo.set_active_text (popdowns[0]);
	} else {
		map_combo.set_active_text (cp.current_binding());
	}

	map_combo.signal_changed().connect (sigc::mem_fun (*this, &GMCPGUI::binding_changed));

	set_spacing (6);
	set_border_width (12);

	Label* label = manage (new Label (_("Available MIDI bindings:")));
	HBox* hpack = manage (new HBox);

	hpack->set_spacing (6);
	hpack->pack_start (*label, false, false);
	hpack->pack_start (map_combo, false, false);

	map_combo.show ();
	label->show ();
	hpack->show ();
	
	pack_start (*hpack, false, false);


	bank_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &GMCPGUI::bank_change));

	label = manage (new Label (_("Current Bank:")));
	hpack = manage (new HBox);

	hpack->set_spacing (6);
	hpack->pack_start (*label, false, false);
	hpack->pack_start (bank_spinner, false, false);


	bank_spinner.show ();
	label->show ();
	hpack->show ();

	pack_start (*hpack, false, false);

}

GMCPGUI::~GMCPGUI ()
{
}

void
GMCPGUI::bank_change ()
{
	int new_bank = bank_adjustment.get_value() - 1;
	cp.set_current_bank (new_bank);
}

void
GMCPGUI::binding_changed ()
{
	string str = map_combo.get_active_text ();

	if (str == _("Reset All")) {
		cp.drop_bindings ();
	} else {
		for (list<GenericMidiControlProtocol::MapInfo>::iterator x = cp.map_info.begin(); x != cp.map_info.end(); ++x) {
			if (str == (*x).name) {
				cp.load_bindings ((*x).path);
				break;
			}
		}
	}
}
