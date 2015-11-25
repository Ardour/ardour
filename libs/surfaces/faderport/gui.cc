/*
    Copyright (C) 2009-2012 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <iostream>
#include <list>
#include <string>

#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "faderport.h"

#include "i18n.h"

namespace ArdourSurface {

class GMCPGUI : public Gtk::VBox
{
public:
	GMCPGUI (FaderPort&);
	~GMCPGUI ();

private:
	FaderPort& cp;
	Gtk::ComboBoxText map_combo;
	Gtk::Adjustment bank_adjustment;
	Gtk::SpinButton bank_spinner;
	Gtk::CheckButton motorised_button;
	Gtk::Adjustment threshold_adjustment;
	Gtk::SpinButton threshold_spinner;

	void binding_changed ();
	void bank_changed ();
	void motorised_changed ();
	void threshold_changed ();
};

}

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

void*
FaderPort::get_gui () const
{
	if (!gui) {
		const_cast<FaderPort*>(this)->build_gui ();
	}
	static_cast<Gtk::VBox*>(gui)->show_all();
	return gui;
}

void
FaderPort::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete (GMCPGUI*) gui;
	gui = 0;
}

void
FaderPort::build_gui ()
{
	gui = (void*) new GMCPGUI (*this);
}

/*--------------------*/

GMCPGUI::GMCPGUI (FaderPort& p)
	: cp (p)
	, bank_adjustment (1, 1, 100, 1, 10)
	, bank_spinner (bank_adjustment)
	, motorised_button ("Motorised")
	, threshold_adjustment (p.threshold(), 1, 127, 1, 10)
	, threshold_spinner (threshold_adjustment)
{
}

GMCPGUI::~GMCPGUI ()
{
}

void
GMCPGUI::bank_changed ()
{
//	int new_bank = bank_adjustment.get_value() - 1;
//	cp.set_current_bank (new_bank);
}

void
GMCPGUI::binding_changed ()
{
}

void
GMCPGUI::motorised_changed ()
{
//	cp.set_motorised (motorised_button.get_active ());
}

void
GMCPGUI::threshold_changed ()
{
//	cp.set_threshold (threshold_adjustment.get_value());
}
