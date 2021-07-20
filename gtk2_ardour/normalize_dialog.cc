/*
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/stock.h>
#include <gtkmm/table.h>
#include <gtkmm/progressbar.h>

#include "normalize_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;

double NormalizeDialog::_last_normalization_value = 0;
double NormalizeDialog::_last_rms_target_value = -9;
double NormalizeDialog::_last_lufs_target_value = -14;

bool NormalizeDialog::_last_normalize_individually = true;
bool NormalizeDialog::_last_constrain_rms = false;
bool NormalizeDialog::_last_constrain_lufs = false;
bool NormalizeDialog::_last_normalize_true_peak = false;

NormalizeDialog::NormalizeDialog (bool more_than_one)
	: ArdourDialog (more_than_one ? _("Normalize regions") : _("Normalize region"))
	, _normalize_individually (0)
{
	get_vbox()->set_spacing (12);

	Table* tbl = manage (new Table);
	tbl->set_spacings (6);
	tbl->set_border_width (6);

	_dbfs_dbtp = manage (new Gtk::ComboBoxText);
	_dbfs_dbtp->append_text (_("dBFS"));
	_dbfs_dbtp->append_text (_("dBTP"));
	_dbfs_dbtp->set_active (_last_normalize_true_peak ? 1 : 0);

	_spin_peak = manage (new SpinButton (0.2, 2));
	_spin_peak->set_range (-112, 0);
	_spin_peak->set_increments (0.1, 1);
	_spin_peak->set_value (_last_normalization_value);
	_spin_peak->set_activates_default ();

	_constrain_rms = manage (new CheckButton (_("Constrain RMS to:")));
	_constrain_rms->set_active (_last_constrain_rms);

	_constrain_lufs = manage (new CheckButton (_("Constrain LUFS to:")));
	_constrain_lufs->set_active (_last_constrain_lufs);

	_spin_rms = manage (new SpinButton (0.2, 2));
	_spin_rms->set_range (-112, 0);
	_spin_rms->set_increments (0.1, 1);
	_spin_rms->set_value (_last_rms_target_value);

	_spin_lufs = manage (new SpinButton (0.2, 2));
	_spin_lufs->set_range (-48, 0);
	_spin_lufs->set_increments (0.5, 1);
	_spin_lufs->set_value (_last_lufs_target_value);

	tbl->attach (*manage (new Label (_("Normalize to:"), ALIGN_END)), 0, 1, 0, 1, FILL, SHRINK);
	tbl->attach (*_spin_peak, 1, 2, 0, 1, SHRINK, SHRINK);
	tbl->attach (*_dbfs_dbtp, 2, 3, 0, 1, SHRINK, SHRINK);

	tbl->attach (*_constrain_rms, 0, 1, 1, 2, SHRINK, SHRINK);
	tbl->attach (*_spin_rms, 1, 2, 1, 2, SHRINK, SHRINK);
	tbl->attach (*manage (new Label (_("dBFS"))), 2, 3, 1, 2, SHRINK, SHRINK);

	tbl->attach (*_constrain_lufs, 0, 1, 2, 3, SHRINK, SHRINK);
	tbl->attach (*_spin_lufs, 1, 2, 2, 3, SHRINK, SHRINK);
	tbl->attach (*manage (new Label (_("LUFS"))), 2, 3, 2, 3, SHRINK, SHRINK);

	get_vbox()->pack_start (*tbl);

	if (more_than_one) {
		RadioButtonGroup group;
		VBox* vbox = manage (new VBox);

		_normalize_individually = manage (new RadioButton (group, _("Normalize each region using its own peak value")));
		vbox->pack_start (*_normalize_individually);
		RadioButton* b = manage (new RadioButton (group, _("Normalize each region using the peak value of all regions")));
		vbox->pack_start (*b);

		_normalize_individually->set_active (_last_normalize_individually);
		b->set_active (!_last_normalize_individually);

		get_vbox()->pack_start (*vbox);
	}

	_progress_bar = manage (new ProgressBar);
	get_vbox()->pack_start (*_progress_bar);

	update_sensitivity ();
	show_all ();
	_progress_bar->hide ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (_("Normalize"), RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);

	_constrain_rms->signal_toggled ().connect (sigc::mem_fun (*this, &NormalizeDialog::update_sensitivity));
	_constrain_lufs->signal_toggled ().connect (sigc::mem_fun (*this, &NormalizeDialog::update_sensitivity));
	signal_response().connect (sigc::mem_fun (*this, &NormalizeDialog::button_clicked));
}

void
NormalizeDialog::update_sensitivity ()
{
	_spin_rms->set_sensitive (constrain_rms ());
	_spin_lufs->set_sensitive (constrain_lufs ());
}

bool
NormalizeDialog::normalize_individually () const
{
	if (_normalize_individually == 0) {
		return true;
	}

	return _normalize_individually->get_active ();
}

bool
NormalizeDialog::constrain_rms () const
{
	return _constrain_rms->get_active ();
}

bool
NormalizeDialog::constrain_lufs () const
{
	return _constrain_lufs->get_active ();
}

bool
NormalizeDialog::use_true_peak () const
{
	return _dbfs_dbtp->get_active_row_number () == 1;
}

double
NormalizeDialog::target_peak () const
{
	return _spin_peak->get_value ();
}

double
NormalizeDialog::target_rms () const
{
	return _spin_rms->get_value ();
}

double
NormalizeDialog::target_lufs () const
{
	return _spin_lufs->get_value ();
}

void
NormalizeDialog::update_progress_gui (float p)
{
	/* Normalization is run inside the GUI thread, so we can directly
	 * update the progress bar when notified about progress.
	 */
	_progress_bar->show ();
	_progress_bar->set_fraction (p);
}

int
NormalizeDialog::run ()
{
	int const r = ArdourDialog::run ();
	_last_normalization_value = target_peak ();
	_last_rms_target_value    = target_rms ();
	_last_lufs_target_value   = target_lufs ();
	_last_constrain_rms       = constrain_rms ();
	_last_constrain_lufs      = constrain_lufs ();
	_last_normalize_true_peak = use_true_peak ();
	if (_normalize_individually) {
		_last_normalize_individually = _normalize_individually->get_active ();
	}
	return r;
}

void
NormalizeDialog::button_clicked (int r)
{
	if (r == RESPONSE_CANCEL) {
		cancel ();
	}
}
