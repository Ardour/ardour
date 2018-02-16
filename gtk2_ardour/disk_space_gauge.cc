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
#include "dsp_load_gauge.h"

#include "pbd/i18n.h"

#define PADDING 3

DiskSpaceGauge::DiskSpaceGauge ()
	: ArdourGauge (">24h")
	, _sec (-1)
{
}

void
DiskSpaceGauge::set_available_disk_sec (float sec)
{
	if (_sec == sec) {
		return;
	}
	_sec = sec;

	if (sec < 0) {
		update (_("N/A"));
		return;
	}

	char buf[64];
	if (_sec > 86400) {
		update (_("Rec: >24h"));
		return;
	} else if (_sec > 32400 /* 9 hours */) {
		snprintf (buf, sizeof (buf), "Rec: %.0fh", _sec / 3600.f);
	} else if (_sec > 5940 /* 99 mins */) {
		snprintf (buf, sizeof (buf), "Rec: %.1fh", _sec / 3600.f);
	} else {
		snprintf (buf, sizeof (buf), "Rec: %.0fm", _sec / 60.f);
	}
	update (std::string (buf));
}

float
DiskSpaceGauge::level () const {
	static const float six_hours = 6.f * 3600.f;
	if (_sec < 0) return 1.0;
	if (_sec > six_hours) return 0.0;
	return (1.0 - (_sec / six_hours));
}

bool
DiskSpaceGauge::alert () const
{
	return _sec >=0 && _sec < 60.f * 10.f;
}

ArdourGauge::Status
DiskSpaceGauge::indicator () const
{
	if (_sec > 3600.f) {
		return ArdourGauge::Level_OK;
	} else if (_sec > 1800.f) {
		return ArdourGauge::Level_WARN;
	}
	return ArdourGauge::Level_CRIT;
}

std::string
DiskSpaceGauge::tooltip_text ()
{
	if (_sec < 0) {
		return _("Unkown");
	}

	int sec = floor (_sec);
	char buf[64];
	int hrs  = sec / 3600;
	int mins = (sec / 60) % 60;
	int secs = sec % 60;

	snprintf (buf, sizeof(buf), _("%02dh:%02dm:%02ds"), hrs, mins, secs);
	return _("Available capture disk-space: ") + std::string (buf);
}
