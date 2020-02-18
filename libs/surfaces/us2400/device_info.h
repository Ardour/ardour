/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_us2400_control_protocol_device_info_h__
#define __ardour_us2400_control_protocol_device_info_h__

#include <iostream>
#include <stdint.h>
#include <string>
#include <map>

#include "button.h"

class XMLNode;

namespace ArdourSurface {

namespace US2400 {

struct GlobalButtonInfo {
	std::string label; // visible to user
	std::string group; // in case we want to present in a GUI
	int32_t id;       // value sent by device

	GlobalButtonInfo () : id (-1) {}
	GlobalButtonInfo (const std::string& l, const std::string& g, uint32_t i)
		: label (l), group (g), id (i) {}
};

struct StripButtonInfo {
	int32_t base_id;
	std::string name;

	StripButtonInfo () : base_id (-1) {}
	StripButtonInfo (uint32_t i, const std::string& n)
		: base_id (i), name (n) {}
};

class DeviceInfo
{
                                        public:
	enum DeviceType {
		MCU = 0x14,
		MCXT = 0x15,
		LC = 0x10,
		LCXT = 0x11,
		HUI = 0x5
	};

	DeviceInfo();
	~DeviceInfo();

	int set_state (const XMLNode&, int version);

	DeviceType device_type() const { return _device_type; }
	uint32_t strip_cnt () const;
	uint32_t extenders() const;
	uint32_t master_position() const;
	bool has_two_character_display() const;
	bool has_master_fader () const;
	bool has_timecode_display() const;
	bool has_global_controls() const;
	bool has_jog_wheel () const;
	bool has_touch_sense_faders() const;
	bool no_handshake() const;
	bool has_meters() const;
	bool has_separate_meters() const;
	bool us2400() const { return _us2400; }
	const std::string& name() const;

	static std::map<std::string,DeviceInfo> device_info;
	static void reload_device_info();

	std::string& get_global_button_name(Button::ID);
	GlobalButtonInfo& get_global_button(Button::ID);

	typedef std::map<Button::ID,GlobalButtonInfo> GlobalButtonsInfo;
	typedef std::map<Button::ID,StripButtonInfo> StripButtonsInfo;

	const GlobalButtonsInfo& global_buttons() const { return _global_buttons; }
	const StripButtonsInfo& strip_buttons() const { return _strip_buttons; }

                                        private:
	uint32_t _strip_cnt;
	uint32_t _extenders;
	uint32_t _master_position;
	bool     _has_two_character_display;
	bool     _has_master_fader;
	bool     _has_timecode_display;
	bool     _has_global_controls;
	bool     _has_jog_wheel;
	bool     _has_touch_sense_faders;
	bool     _uses_logic_control_buttons;
	bool     _no_handshake;
	bool     _has_meters;
	bool     _has_separate_meters;
	bool     _us2400;
	DeviceType _device_type;
	std::string _name;
	std::string _global_button_name;

	GlobalButtonsInfo _global_buttons;
	StripButtonsInfo _strip_buttons;

	void logic_control_buttons ();
	void us2400_control_buttons ();
	void shared_buttons ();
};


} // US2400 namespace
} // ArdourSurface namespace

std::ostream& operator<< (std::ostream& os, const ArdourSurface::US2400::DeviceInfo& di);

#endif /* __ardour_us2400_control_protocol_device_info_h__ */
