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

#ifndef __gtkardour_disk_space_indicator_h__
#define __gtkardour_disk_space_indicator_h__

#include <pangomm.h>

#include "ardour_gauge.h"

class DiskSpaceIndicator : public ArdourGauge
{
public:
	DiskSpaceIndicator ();

	void set_available_disk_sec (float);

protected:
	bool alert () const;
	ArdourGauge::Status indicator () const;
	float level () const;
	std::string tooltip_text ();

private:
	float _sec;
};

#endif
