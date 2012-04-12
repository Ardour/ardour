/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "pbd/xml++.h"

#include "device_info.h"

#include "i18n.h"

using namespace Mackie;
using namespace PBD;
using std::string;

DeviceInfo::DeviceInfo()
	: _strip_cnt (8)
	, _has_two_character_display (true)
	, _has_master_fader (true)
	, _has_segmented_display (false)
	, _has_timecode_display (true)
	, _name (X_("Mackie Control Universal Pro"))
{
	
}

DeviceInfo::DeviceInfo (const XMLNode& node)
	: _strip_cnt (8)
	, _has_two_character_display (true)
	, _has_master_fader (true)
	, _has_segmented_display (false)
	, _has_timecode_display (true)
	, _name (X_("Mackie Control Universal Pro"))
{
	const XMLProperty* prop;

	if ((prop = node.property ("strips")) != 0) {
		if ((_strip_cnt = atoi (prop->value())) == 0) {
			_strip_cnt = 8;
		}
	}

	if ((prop = node.property ("two-character-display")) != 0) {
		_has_two_character_display = string_is_affirmative (prop->value());
	}

	if ((prop = node.property ("master-fader")) != 0) {
		_has_master_fader = string_is_affirmative (prop->value());
	}

	if ((prop = node.property ("display-segmented")) != 0) {
		_has_segmented_display = string_is_affirmative (prop->value());
	}

	if ((prop = node.property ("timecode-display")) != 0) {
		_has_timecode_display = string_is_affirmative (prop->value());
	}

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	}
}

uint32_t
DeviceInfo::strip_cnt() const
{
	return _strip_cnt;
}

bool
DeviceInfo::has_master_fader() const
{
	return _has_master_fader;
}

bool
DeviceInfo::has_two_character_display() const
{
	return _has_two_character_display;
}

bool
DeviceInfo::has_segmented_display() const
{
	return _has_segmented_display;
}

bool
DeviceInfo::has_timecode_display () const
{
	return _has_timecode_display;
}

