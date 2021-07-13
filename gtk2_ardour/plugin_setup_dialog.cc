/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>
#include <gtkmm/table.h>

#include "plugin_setup_dialog.h"
#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtk;

PluginSetupDialog::PluginSetupDialog (boost::shared_ptr<ARDOUR::Route> r, boost::shared_ptr<ARDOUR::PluginInsert> pi, ARDOUR::Route::PluginSetupOptions flags)
	: ArdourDialog (_("Plugin Setup"), true, false)
	, _route (r)
	, _pi (pi)
	, _keep_mapping (_("Copy I/O Map"), ArdourButton::led_default_elements)
	, _fan_out (_("Fan out"), ArdourButton::led_default_elements)
{
	assert (flags != Route::None);

	Gtk::Table *tbl = manage (new Gtk::Table ());
	tbl->set_spacings (6);
	get_vbox()->pack_start (*tbl);
	int row = 0;

	if (flags & Route::CanReplace) {
		boost::shared_ptr<Processor> old = _route->the_instrument ();
		boost::shared_ptr<PluginInsert> opi = boost::dynamic_pointer_cast<PluginInsert> (old);
		assert (opi);

		opi->configured_io (_cur_inputs, _cur_outputs);

		Gtk::Label* l = manage (new Label (
						_("An Instrument plugin is already present.")
						));
		tbl->attach (*l, 0, 2, row, row + 1, EXPAND|FILL, SHRINK); ++row;

		l = manage (new Label (_("Replace"), ALIGN_END));
		tbl->attach (*l, 0, 1, row, row + 1, EXPAND|FILL, SHRINK);

		l = manage (new Label (string_compose ("'%1'", old->name ()), ALIGN_START));
		tbl->attach (*l, 1, 2, row, row + 1, EXPAND|FILL, SHRINK); ++row;

		l = manage (new Label (_("with"), ALIGN_END));
		tbl->attach (*l, 0, 1, row, row + 1, EXPAND|FILL, SHRINK);

		l = manage (new Label (string_compose ("'%1'", pi->name ()), ALIGN_START));
		tbl->attach (*l, 1, 2, row, row + 1, EXPAND|FILL, SHRINK); ++row;

		Box* box = manage (new HBox ());
		box->set_border_width (2);
		box->pack_start (_keep_mapping, true, true);
		Frame* f = manage (new Frame ());
		f->set_label (_("I/O Pin Mapping"));
		f->add (*box);
		tbl->attach (*f, 0, 1, row, row + 1, EXPAND|FILL, SHRINK, 0, 8);

		_keep_mapping.signal_clicked.connect (sigc::mem_fun (*this, &PluginSetupDialog::apply_mapping));
		add_button (_("Replace"), 2);
	} else {

		Gtk::Label *l = manage (new Label (string_compose (
						_("Configure Plugin '%1'"), pi->name ()
						)));
		tbl->attach (*l, 0, 2, row, row + 1, EXPAND|FILL, SHRINK); ++row;
	}

	if (flags & Route::MultiOut) {
		setup_output_presets ();
		Box* box = manage (new HBox ());
		box->set_border_width (2);
		box->pack_start (_out_presets, true, true);
		box->pack_start (_fan_out, false, false);
		Frame* f = manage (new Frame ());
		f->set_label (_("Output Configuration"));
		f->add (*box);
		tbl->attach (*f, 1, 2, row, row + 1, EXPAND|FILL, SHRINK, 0, 8);
		_fan_out.signal_clicked.connect (sigc::mem_fun (*this, &PluginSetupDialog::toggle_fan_out));
		_fan_out.set_active (true);
	} else {
		_pi->set_preset_out (_pi->natural_output_streams ());
		update_sensitivity (_pi->natural_output_streams ().n_audio ());
		_fan_out.set_active (false);
	}

	_keep_mapping.set_active (false);
	apply_mapping ();

	add_button (Stock::ADD, 0);
	add_button (Stock::CANCEL, 1);
	set_default_response (0);
	show_all ();
}


void
PluginSetupDialog::setup_output_presets ()
{
	// compare to PluginPinDialog::refill_output_presets ()
	using namespace Menu_Helpers;
	PluginOutputConfiguration ppc (_pi->plugin (0)->possible_output ());

	_out_presets.AddMenuElem (MenuElem (_("Automatic"), sigc::bind (sigc::mem_fun (*this, &PluginSetupDialog::select_output_preset), 0)));

	if (ppc.find (0) != ppc.end ()) {
		// anything goes
		ppc.clear ();
		ppc.insert (1);
		ppc.insert (2);
		ppc.insert (8);
		ppc.insert (16);
		ppc.insert (24);
		ppc.insert (32);
		if (ppc.find (_cur_outputs.n_audio ()) == ppc.end ()) {
			ppc.insert (_cur_outputs.n_audio ());
		}
	}

	bool have_matching_io = false;

	for (PluginOutputConfiguration::const_iterator i = ppc.begin () ; i != ppc.end (); ++i) {
		assert (*i > 0);
		_out_presets.AddMenuElem (MenuElem (preset_label (*i), sigc::bind (sigc::mem_fun (*this, &PluginSetupDialog::select_output_preset), *i)));
		if (*i == _cur_outputs.n_audio ()) {
			have_matching_io = true;
		}
	}

	if (have_matching_io) {
		select_output_preset (_cur_outputs.n_audio ());
	} else if (ppc.size() == 1 && _pi->strict_io ()) {
		select_output_preset (*ppc.begin ());
	} else {
		select_output_preset (0);
	}
}

void
PluginSetupDialog::select_output_preset (uint32_t n_audio)
{
	_pi->set_preset_out (ChanCount (DataType::AUDIO, n_audio));
	_out_presets.set_text (preset_label (n_audio));
	update_sensitivity (n_audio);
}

void
PluginSetupDialog::update_sensitivity (uint32_t n_audio)
{
	if (_cur_outputs.n_audio () > 0 && _cur_outputs.n_audio () == n_audio) {
		// TODO check _cur_inputs if not reconfigurable?
		_keep_mapping.set_sensitive (true);
	} else {
		_keep_mapping.set_sensitive (false);
	}
	_fan_out.set_sensitive (n_audio > 2);
}

bool
PluginSetupDialog::io_match () const
{
	if (_cur_outputs.n_audio () > 0 && _cur_outputs.n_audio () == _pi->preset_out ().n_audio ()) {
		return true;
	} else {
		return false;
	}
}

void
PluginSetupDialog::apply_mapping ()
{
	// toggle button
	_keep_mapping.set_active (!_keep_mapping.get_active ());

	boost::shared_ptr<Processor> old = _route->the_instrument ();
	boost::shared_ptr<PluginInsert> opi = boost::dynamic_pointer_cast<PluginInsert> (old);

	if (_keep_mapping.get_active () && opi && io_match ()) {
		_pi->pre_seed (_cur_inputs, _cur_outputs, opi->input_map (0), opi->output_map (0), opi->thru_map ());
	} else {
		_pi->pre_seed (ChanCount (), ChanCount (), ChanMapping (), ChanMapping (), ChanMapping());
	}
}

void
PluginSetupDialog::toggle_fan_out ()
{
	_fan_out.set_active (!_fan_out.get_active ());
}

std::string
PluginSetupDialog::preset_label (uint32_t n_audio) const
{
		std::string rv;
		switch (n_audio) {
			case 0:
				rv = _("Automatic");
				break;
			case 1:
				rv = _("Mono");
				break;
			case 2:
				rv = _("Stereo");
				break;
			default:
				rv = string_compose (P_("%1 Channel", "%1 Channels", n_audio), n_audio);
				break;
		}
		return rv;
}
