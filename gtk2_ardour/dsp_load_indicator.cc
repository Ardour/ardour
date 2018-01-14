/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ardour_ui.h"
#include "dsp_load_indicator.h"

#include "pbd/i18n.h"

#define PADDING 3

DspLoadIndicator::DspLoadIndicator ()
	: ArdourGauge ("00.0%")
	, _dsp_load (0)
	, _xrun_count (0)
{
}

void
DspLoadIndicator::set_xrun_count (const unsigned int xruns)
{
	if (xruns == _xrun_count) {
		return;
	}
	_xrun_count = xruns;
	update ();
}

void
DspLoadIndicator::set_dsp_load (const double load)
{
	if (load == _dsp_load) {
		return;
	}
	_dsp_load = load;

	char buf[64];
	snprintf (buf, sizeof (buf), "%.1f%%", _dsp_load);
	update (std::string (buf));
}

float
DspLoadIndicator::level () const {
	return _dsp_load / 100.f;
}

bool
DspLoadIndicator::alert () const
{
	return _xrun_count > 0;
}

ArdourGauge::Status
DspLoadIndicator::indicator () const
{
	if (_dsp_load > 90) {
		return ArdourGauge::Level_CRIT;
	} else if (_dsp_load > 80) {
		return ArdourGauge::Level_WARN;
	} else {
		return ArdourGauge::Level_OK;
	}
}

std::string
DspLoadIndicator::tooltip_text ()
{
	char buf[64];
	if (_xrun_count == UINT_MAX) {
		snprintf (buf, sizeof (buf), _("DSP: %.1f%% X: ?"), _dsp_load);
	} else if (_xrun_count > 9999) {
		snprintf (buf, sizeof (buf), _("DSP: %.1f%% X: >10k"), _dsp_load);
	} else {
		snprintf (buf, sizeof (buf), _("DSP: %.1f%% X: %u"), _dsp_load, _xrun_count);
	}
	return buf;
}

bool
DspLoadIndicator::on_button_release_event (GdkEventButton *ev)
{
	ARDOUR::Session* s = ARDOUR_UI::instance ()->the_session ();
	if (s) {
		s->reset_xrun_count ();
	}
	return true;
}
