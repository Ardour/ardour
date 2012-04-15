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

#ifndef __ardour_mackie_control_protocol_device_info_h__
#define __ardour_mackie_control_protocol_device_info_h__

#include <iostream>
#include <stdint.h>
#include <string>
#include <map>

#include "button.h"

class XMLNode;

namespace Mackie {

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
	DeviceInfo();
	~DeviceInfo();

	int set_state (const XMLNode&, int version);

	uint32_t strip_cnt () const;
	uint32_t extenders() const;
	bool has_two_character_display() const; 
	bool has_master_fader () const;
	bool has_segmented_display() const;
	bool has_timecode_display() const;
	bool has_global_controls() const;
	bool has_jog_wheel () const;
	bool has_touch_sense_faders() const;
	const std::string& name() const;

	static std::map<std::string,DeviceInfo> device_info;
	static void reload_device_info();

    const std::map<Button::ID,GlobalButtonInfo>& global_buttons() const { return _global_buttons; }
    const std::map<Button::ID,StripButtonInfo>& strip_buttons() const { return _strip_buttons; }
	
  private:
    uint32_t _strip_cnt;
    uint32_t _extenders;
    bool     _has_two_character_display;
    bool     _has_master_fader;
    bool     _has_segmented_display;
    bool     _has_timecode_display;
    bool     _has_global_controls;
    bool     _has_jog_wheel;
    bool     _has_touch_sense_faders;
    bool     _uses_logic_control_buttons;
    std::string _name;

    std::map<Button::ID,GlobalButtonInfo> _global_buttons;
    std::map<Button::ID,StripButtonInfo>  _strip_buttons;

    void logic_control_buttons ();
    void mackie_control_buttons ();
};

class DeviceProfile
{
  public:
	DeviceProfile (DeviceInfo& info);
	~DeviceProfile();

	const std::string& get_f_action (uint32_t fn, int modifier_state);
	void set_f_action (uint32_t fn, int modifier_state, const std::string&);
};

}

std::ostream& operator<< (std::ostream& os, const Mackie::DeviceInfo& di);

#endif /* __ardour_mackie_control_protocol_device_info_h__ */
