/*
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Paul Davis
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __libardouralsautil_deviceinfo_h__
#define __libardouralsautil_deviceinfo_h__

namespace ARDOUR {

	struct ALSADeviceInfo {
		unsigned int max_channels;
		unsigned int min_rate, max_rate;
		unsigned long min_size, max_size;
		bool valid;
	};

	int get_alsa_device_parameters (const char* device_name, const bool play, ALSADeviceInfo *nfo);
}
#endif
