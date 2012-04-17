/*
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

#ifndef __ardour_mackie_control_protocol_device_profile_h__
#define __ardour_mackie_control_protocol_device_profile_h__

#include <iostream>
#include <stdint.h>
#include <string>
#include <map>

#include "button.h"

class XMLNode;

namespace Mackie {

class DeviceProfile
{
  public:
	DeviceProfile (const std::string& name = "");
	~DeviceProfile();
	
	std::string get_button_action (Button::ID, int modifier_state) const;
	void set_button_action (Button::ID, int modifier_state, const std::string&);
	
	const std::string& name() const;
	
	static void reload_device_profiles ();
	static std::map<std::string,DeviceProfile> device_profiles;
	
  private:
	struct ButtonActions {
	    std::string plain;
	    std::string control;
	    std::string shift;
	    std::string option;
	    std::string cmdalt;
	    std::string shiftcontrol;
	};
	
	typedef std::map<Button::ID,ButtonActions> ButtonActionMap;
	
	std::string _name;
	ButtonActionMap _button_map;
	
	int set_state (const XMLNode&, int version);
	XMLNode& get_state () const;
};

}

#endif /* __ardour_mackie_control_protocol_device_profile_h__ */
