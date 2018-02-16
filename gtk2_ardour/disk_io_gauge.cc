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
#include "disk_io_gauge.h"

#include "ardour/audioengine.h"

#include "pbd/i18n.h"

#define PADDING 3

DiskIoGauge::DiskIoGauge ()
	: ArdourGauge ("00.0%")
	, _disk_play (0)
	, _disk_capture (0)
{
}

void
DiskIoGauge::set_disk_io (const double play, const double capture)
{
	if (play == _disk_play && capture == _disk_capture) {
		return;
	}
	_disk_play = 100.0-play;
	_disk_capture = 100.0-capture;

	char buf[64];
	if ( _disk_play > 1.0 && _disk_play < 10.0 && _disk_capture < 2.0 ) {
		snprintf (buf, sizeof (buf), "Disk:  %.0f%% / 0%%", _disk_play);
	} else if ( _disk_play > 1.0 && _disk_capture < 2.0 ) {
		snprintf (buf, sizeof (buf), "Disk: %.0f%% / 0%%", _disk_play);
	} else if ( _disk_play > 1.0 && _disk_capture > 1.0 ) {
		snprintf (buf, sizeof (buf), "Disk: %.0f%% / %.0f%%", _disk_play, _disk_capture);
	} else {
		snprintf (buf, sizeof (buf), " ");
	}
	update (std::string (buf));
}

float
DiskIoGauge::level () const {
	return min ( _disk_play / 100.f, _disk_capture / 100.f);
}

bool
DiskIoGauge::alert () const
{
	return false;
}

ArdourGauge::Status
DiskIoGauge::indicator () const
{
	float lvl = level();
	
	if (lvl > 0.6) {
		return ArdourGauge::Level_CRIT;
	} else if (lvl > 0.4) {
		return ArdourGauge::Level_WARN;
	} else {
		return ArdourGauge::Level_OK;
	}
}

std::string
DiskIoGauge::tooltip_text ()
{
	char buf[128];

	snprintf (buf, sizeof (buf), "Disk Play/Record cache: %.0f%% / %.0f%%", _disk_play, _disk_capture);

	return buf;
}
