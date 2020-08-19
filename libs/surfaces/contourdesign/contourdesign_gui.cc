/*
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <libusb.h>

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>

#include "pbd/unwind.h"

#include "ardour/debug.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "widgets/ardour_button.h"

#include "contourdesign.h"
#include "jump_distance_widget.h"
#include "button_config_widget.h"

#include "pbd/i18n.h"

using namespace ArdourSurface;

class ContourDesignGUI : public Gtk::VBox, public PBD::ScopedConnectionList
{
public:
	ContourDesignGUI (ContourDesignControlProtocol& ccp);
	~ContourDesignGUI () {}

private:
	ContourDesignControlProtocol& _ccp;

	ArdourWidgets::ArdourButton _test_button;

	Gtk::CheckButton _keep_rolling;
	void toggle_keep_rolling ();

	std::vector<boost::shared_ptr<Gtk::Adjustment> > _shuttle_speed_adjustments;
	void set_shuttle_speed (int index);

	JumpDistanceWidget _jog_distance;
	void update_jog_distance ();

	void update_action(unsigned int index, ButtonConfigWidget* sender);

	void toggle_test_mode ();

	void test_button_press (unsigned short btn);
	void test_button_release (unsigned short btn);

	std::vector<boost::shared_ptr<ArdourWidgets::ArdourButton> > _btn_leds;

	void init_on_show ();
	bool reset_test_state (GdkEventAny* = 0);
	bool update_device_state ();

	Gtk::Label _device_state_lbl;

	sigc::signal<void, bool> ProButtonsSensitive;
	sigc::signal<void, bool> XpressButtonsSensitive;
};


using namespace PBD;
using namespace ARDOUR;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ArdourWidgets;


ContourDesignGUI::ContourDesignGUI (ContourDesignControlProtocol& ccp)
	: _ccp (ccp)
	, _test_button (_("Button Test"), ArdourButton::led_default_elements)
	, _keep_rolling (_("Keep rolling after jumps"))
	, _jog_distance (ccp.jog_distance ())
	, _device_state_lbl ()
{
	Frame* dg_sample = manage (new Frame (_("Device")));
	dg_sample->set_size_request (300, -1);
	VBox* dg_box = manage (new VBox);
	dg_sample->add (*dg_box);
	dg_box->set_border_width (6);
	dg_box->pack_start (_device_state_lbl);

	_device_state_lbl.set_line_wrap (true);

	Frame* sj_sample = manage (new Frame (_("Shuttle speeds and jog jump distances")));
	Table* sj_table = manage (new Table);
	sj_sample->set_border_width (6);
	sj_table->set_border_width (12);
	sj_sample->add (*sj_table);

	Label* speed_label = manage (new Label (_("Transport speeds for the shuttle positions:"), ALIGN_START));
	sj_table->attach (*speed_label, 0,1, 0,1, FILL|EXPAND, FILL|EXPAND, /* xpadding = */ 12);

	HBox* speed_box = manage (new HBox);
	for (int i=0; i != ContourDesignControlProtocol::num_shuttle_speeds; ++i) {
		double speed = ccp.shuttle_speed (i);
		boost::shared_ptr<Gtk::Adjustment> adj (new Gtk::Adjustment (speed, 0.0, 100.0, 0.25));
		_shuttle_speed_adjustments.push_back (adj);
		SpinButton* sb = manage (new SpinButton (*adj, 0.25, 2));
		speed_box->pack_start (*sb);
		sb->signal_value_changed().connect (sigc::bind (sigc::mem_fun(*this, &ContourDesignGUI::set_shuttle_speed), i));
	}
	sj_table->attach (*speed_box, 1,2, 0,1);

	Label* jog_label = manage (new Label (_("Jump distance for jog wheel:"), ALIGN_START));
	_jog_distance.Changed.connect (sigc::mem_fun (*this, &ContourDesignGUI::update_jog_distance));

	sj_table->attach (*jog_label, 0,1, 1,2, FILL|EXPAND, FILL|EXPAND, /* xpadding = */ 12);
	sj_table->attach (_jog_distance, 1,2, 1,2);

	_keep_rolling.set_tooltip_text (_("If checked Ardour keeps rolling after jog or shuttle events. If unchecked it stops."));
	_keep_rolling.signal_toggled().connect (sigc::mem_fun (*this, &ContourDesignGUI::toggle_keep_rolling));
	_keep_rolling.set_active (_ccp.keep_rolling ());

	sj_table->attach (_keep_rolling, 0,1, 2,3);


	Frame* btn_action_sample = manage (new Frame (_("Actions or jumps for buttons")));
	HBox* btn_action_box = manage (new HBox);
	btn_action_sample->set_border_width (6);
	btn_action_box->set_border_width (12);
	btn_action_sample->add (*btn_action_box);

	VBox* tbb = manage (new VBox);
	_test_button.set_tooltip_text (_("If the button is active, all the button presses are not handled, "
					 "but in the corresponding line in the button table the LED will light up."));
	_test_button.signal_clicked.connect (sigc::mem_fun (*this, &ContourDesignGUI::toggle_test_mode));
	_test_button.set_size_request (-1, 64);
	tbb->pack_start(_test_button, true, false);
	btn_action_box->pack_start (*tbb, true, false, 12);


	Table* table = manage (new Table);
	table->set_row_spacings (6);
	table->set_col_spacings (6);;

	for (int btn_idx=0; btn_idx < _ccp.get_button_count(); ++btn_idx) {
		boost::shared_ptr<ArdourButton> b (new ArdourButton (string_compose (_("Setting for button %1"), btn_idx+1),
								     ArdourButton::Element(ArdourButton::Indicator|ArdourButton::Text|ArdourButton::Inactive)));
		table->attach (*b, 0, 2, btn_idx, btn_idx+1);
		_btn_leds.push_back (b);

		ButtonConfigWidget* bcw = manage (new ButtonConfigWidget);

		boost::shared_ptr<ButtonBase> btn_act = _ccp.get_button_action (btn_idx);
		assert (btn_act);
		bcw->set_current_config (btn_act);

		bcw->Changed.connect (sigc::bind (sigc::mem_fun (*this, &ContourDesignGUI::update_action), btn_idx, bcw));
		table->attach (*bcw, 3, 5, btn_idx, btn_idx+1);

		if (btn_idx > 3 && btn_idx < 9) {
			this->XpressButtonsSensitive.connect (sigc::mem_fun (*b, &ArdourButton::set_sensitive));
			this->XpressButtonsSensitive.connect (sigc::mem_fun (*bcw, &ButtonConfigWidget::set_sensitive));
		} else {
			this->ProButtonsSensitive.connect (sigc::mem_fun (*b, &ArdourButton::set_sensitive));
			this->ProButtonsSensitive.connect (sigc::mem_fun (*bcw, &ButtonConfigWidget::set_sensitive));
		}
	}

	set_spacing (6);
	btn_action_box->pack_start (*table, false, false);

	HBox* top_box = manage (new HBox);
	top_box->pack_start (*dg_sample);
	top_box->pack_start (*sj_sample);
	pack_start (*top_box);
	pack_start (*btn_action_sample);

	_ccp.ButtonPress.connect (*this, invalidator (*this), boost::bind (&ContourDesignGUI::test_button_press, this, _1), gui_context ());
	_ccp.ButtonRelease.connect (*this, invalidator (*this), boost::bind (&ContourDesignGUI::test_button_release, this, _1), gui_context ());

	signal_map().connect (sigc::mem_fun (*this, &ContourDesignGUI::init_on_show));
	update_device_state ();
}

void
ContourDesignGUI::toggle_keep_rolling ()
{
	_ccp.set_keep_rolling (_keep_rolling.get_active ());
}

void
ContourDesignGUI::set_shuttle_speed (int index)
{
	double speed = _shuttle_speed_adjustments[index]->get_value ();
	_ccp.set_shuttle_speed (index, speed);
}

void
ContourDesignGUI::update_jog_distance ()
{
	_ccp.set_jog_distance (_jog_distance.get_distance ());
}

void
ContourDesignGUI::update_action (unsigned int index, ButtonConfigWidget* sender)
{
	_ccp.set_button_action (index, sender->get_current_config (_ccp));
}

void
ContourDesignGUI::toggle_test_mode ()
{
	bool testmode = ! _ccp.test_mode(); // toggle
	_ccp.set_test_mode (testmode);
	if (testmode) {
		_test_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		reset_test_state ();
	}
}

void
ContourDesignGUI::init_on_show ()
{
	Gtk::Widget* p = get_parent();
	if (p) {
		p->signal_delete_event().connect (sigc::mem_fun (*this, &ContourDesignGUI::reset_test_state));
	}
}

bool
ContourDesignGUI::reset_test_state (GdkEventAny*)
{
	_ccp.set_test_mode (false);
	_test_button.set_active (Gtkmm2ext::Off);
	vector<boost::shared_ptr<ArdourButton> >::const_iterator it;
	for (it = _btn_leds.begin(); it != _btn_leds.end(); ++it) {
		(*it)->set_active_state (Gtkmm2ext::Off);
	}

	return false;
}

void
ContourDesignGUI::test_button_press (unsigned short btn)
{
	_btn_leds[btn]->set_active_state (Gtkmm2ext::ExplicitActive);
}

void
ContourDesignGUI::test_button_release (unsigned short btn)
{
	_btn_leds[btn]->set_active_state (Gtkmm2ext::Off);
}

bool
ContourDesignGUI::update_device_state ()
{
	switch (_ccp.device_type ()) {
	case ContourDesignControlProtocol::ShuttlePRO:
		_device_state_lbl.set_markup ("<span weight=\"bold\" foreground=\"green\">Found ShuttlePRO</span>");
		XpressButtonsSensitive (true);
		ProButtonsSensitive (true);
		break;
	case ContourDesignControlProtocol::ShuttlePRO_v2:
		_device_state_lbl.set_markup ("<span weight=\"bold\" foreground=\"green\">Found ShuttlePRO v2</span>");
		XpressButtonsSensitive (true);
		ProButtonsSensitive (true);
		break;
	case ContourDesignControlProtocol::ShuttleXpress:
		_device_state_lbl.set_markup ("<span weight=\"bold\" foreground=\"green\">Found shuttleXpress</span>");
		XpressButtonsSensitive (true);
		ProButtonsSensitive (false);
		break;
	default:
		XpressButtonsSensitive (false);
		ProButtonsSensitive (false);
		_device_state_lbl.set_markup (string_compose ("<span weight=\"bold\" foreground=\"red\">Device not working:</span> %1",
		                              libusb_strerror ((libusb_error)_ccp.usb_errorcode ())));
	}

	return false;
}

void*
ContourDesignControlProtocol::get_gui () const
{
	if (!_gui) {
		const_cast<ContourDesignControlProtocol*>(this)->build_gui ();
	}
	_gui->show_all();
	return (void*) _gui;
}

void
ContourDesignControlProtocol::build_gui ()
{
	_gui = new ContourDesignGUI (*this);
}

void
ContourDesignControlProtocol::tear_down_gui ()
{
	if (_gui) {
		Gtk::Widget *w = _gui->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete _gui;
	_gui = 0;
}
