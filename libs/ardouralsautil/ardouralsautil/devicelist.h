/*
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#ifndef __libardouralsautil_devicelist_h__
#define __libardouralsautil_devicelist_h__

#include <string>
#include <map>
namespace ARDOUR {
	enum AlsaDuplex {
		HalfDuplexIn  = 1,
		HalfDuplexOut = 2,
		FullDuplex    = 3,
	};

	void get_alsa_audio_device_names (std::map<std::string, std::string>& devices, AlsaDuplex duplex = FullDuplex);
	void get_alsa_rawmidi_device_names (std::map<std::string, std::string>& devices);
	void get_alsa_sequencer_names (std::map<std::string, std::string>& devices);
	int card_to_num(const char* device_name);

}
#endif
