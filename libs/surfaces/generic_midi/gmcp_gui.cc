#include <iostream>
#include <list>
#include <string>

#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>

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

    void binding_changed ();
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
{
	vector<string> popdowns;
	popdowns.push_back (_("Reset All"));

	for (list<GenericMidiControlProtocol::MapInfo>::iterator x = cp.map_info.begin(); x != cp.map_info.end(); ++x) {
		popdowns.push_back ((*x).name);
	}

	set_popdown_strings (map_combo, popdowns, true, 5, 2);
	
	if (cp.current_binding().empty()) {
		map_combo.set_active_text (popdowns[0]);
	} else {
		map_combo.set_active_text (cp.current_binding());
	}

	map_combo.signal_changed().connect (sigc::mem_fun (*this, &GMCPGUI::binding_changed));

	set_border_width (12);

	Label* label = manage (new Label (_("Available MIDI bindings:")));
	HBox* hpack = manage (new HBox);

	hpack->set_spacing (6);
	hpack->pack_start (*label, false, false);
	hpack->pack_start (map_combo, false, false);

	pack_start (*hpack, false, false);

	map_combo.show ();
	label->show ();
	hpack->show ();
}

GMCPGUI::~GMCPGUI ()
{
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
