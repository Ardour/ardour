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
	, _disk_io (0)
{
}

void
DiskIoGauge::set_disk_io (const double load)
{
	if (load == _disk_io) {
		return;
	}
	_disk_io = load;

	char buf[64];
	snprintf (buf, sizeof (buf), "Dsk: %.1f%%", _disk_io);
	update (std::string (buf));
}

float
DiskIoGauge::level () const {
	return (_disk_io / 100.f);
}

bool
DiskIoGauge::alert () const
{
	return false;
}

ArdourGauge::Status
DiskIoGauge::indicator () const
{
	if (_disk_io < 50) {
		return ArdourGauge::Level_CRIT;
	} else if (_disk_io < 75) {
		return ArdourGauge::Level_WARN;
	} else {
		return ArdourGauge::Level_OK;
	}
}

std::string
DiskIoGauge::tooltip_text ()
{
	char buf[64];

	snprintf (buf, sizeof (buf), _("Disk I/O cache: %.1f"), _disk_io);

	return buf;
}
