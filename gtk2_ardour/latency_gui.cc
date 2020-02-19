/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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

#include <inttypes.h>
#include <iomanip>

#include <gtkmm/stock.h>

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/unwind.h"

#include "ardour/latent.h"

#include "gtkmm2ext/utils.h"

#include "latency_gui.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;


static const gchar *_unit_strings[] = {
	N_("sample"),
	N_("msec"),
	N_("period"),
	0
};

std::vector<std::string> LatencyGUI::unit_strings;

std::string
LatencyBarController::get_label (double&)
{
	return ARDOUR_UI_UTILS::samples_as_time_string (
			_latency_gui->adjustment.get_value(), _latency_gui->sample_rate, true);
}

void
LatencyGUIControllable::set_value (double v, PBD::Controllable::GroupControlDisposition group_override)
{
	_latency_gui->adjustment.set_value (v);
}

double
LatencyGUIControllable::get_value () const
{
	return _latency_gui->adjustment.get_value ();
}
double
LatencyGUIControllable::lower() const
{
	return _latency_gui->adjustment.get_lower ();
}

double
LatencyGUIControllable::upper() const
{
	return _latency_gui->adjustment.get_upper ();
}

LatencyGUI::LatencyGUI (Latent& l, samplepos_t sr, samplepos_t psz)
	: _latent (l)
	, sample_rate (sr)
	, period_size (psz)
	, _ignore_change (false)
	, adjustment (0, 0.0, sample_rate, 1.0, sample_rate / 1000.0f) /* max 1 second, step by samples, page by msecs */
	, bc (adjustment, this)
	, reset_button (_("Reset"))
{
	Widget* w;

	if (unit_strings.empty()) {
		unit_strings = I18N (_unit_strings);
	}

	set_popdown_strings (units_combo, unit_strings);
	units_combo.set_active_text (unit_strings.front());

	w = manage (new Image (Stock::ADD, ICON_SIZE_BUTTON));
	w->show ();
	plus_button.add (*w);
	w = manage (new Image (Stock::REMOVE, ICON_SIZE_BUTTON));
	w->show ();
	minus_button.add (*w);

	hbox1.pack_start (bc, true, true);

	hbox2.set_homogeneous (false);
	hbox2.set_spacing (12);
	hbox2.pack_start (reset_button);
	hbox2.pack_start (minus_button);
	hbox2.pack_start (plus_button);
	hbox2.pack_start (units_combo, true, true);

	minus_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (*this, &LatencyGUI::change_latency_from_button), -1));
	plus_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (*this, &LatencyGUI::change_latency_from_button), 1));
	reset_button.signal_clicked().connect (sigc::mem_fun (*this, &LatencyGUI::reset));

	/* Limit value to adjustment range (max = sample_rate).
	 * Otherwise if the signal_latency() is larger than the adjustment's max,
	 * LatencyGUI::finish() would set the adjustment's max value as custom-latency.
	 */
	adjustment.set_value (std::min (sample_rate, _latent.signal_latency ()));

	adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &LatencyGUI::finish));

	bc.set_size_request (-1, 25);
	bc.set_name (X_("ProcessorControlSlider"));

	set_spacing (12);
	pack_start (hbox1, true, true);
	pack_start (hbox2, true, true);
}

void
LatencyGUI::finish ()
{
	if (_ignore_change) {
		return;
	}
	samplepos_t new_value = (samplepos_t) adjustment.get_value();
	_latent.set_user_latency (new_value);
}

void
LatencyGUI::reset ()
{
	_latent.unset_user_latency ();
	PBD::Unwinder<bool> uw (_ignore_change, true);
	adjustment.set_value (std::min (sample_rate, _latent.signal_latency ()));
}

void
LatencyGUI::refresh ()
{
	PBD::Unwinder<bool> uw (_ignore_change, true);
	adjustment.set_value (std::min (sample_rate, _latent.effective_latency ()));
}

void
LatencyGUI::change_latency_from_button (int dir)
{
	std::string unitstr = units_combo.get_active_text();
	double shift = 0.0;

	if (unitstr == unit_strings[0]) {
		shift = 1;
	} else if (unitstr == unit_strings[1]) {
		shift = (sample_rate / 1000.0);
	} else if (unitstr == unit_strings[2]) {
		shift = period_size;
	} else {
		fatal << string_compose (_("programming error: %1 (%2)"), X_("illegal string in latency GUI units combo"), unitstr)
		      << endmsg;
		abort(); /*NOTREACHED*/
	}

	if (dir > 0) {
		adjustment.set_value (adjustment.get_value() + shift);
	} else {
		adjustment.set_value (adjustment.get_value() - shift);
	}
}
