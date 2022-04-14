/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_plugin_insert_base_h_
#define _ardour_plugin_insert_base_h_

#include "pbd/destructible.h"

#include "evoral/ControlSet.h"

#include "ardour/ardour.h"
#include "ardour/plugin.h"
#include "ardour/plugin_types.h"

namespace ARDOUR {

class Session;
class ReadOnlyControl;
class Route;
class Plugin;

class LIBARDOUR_API PlugInsertBase : virtual public Evoral::ControlSet, virtual public PBD::Destructible
{
public:
	virtual ~PlugInsertBase () {}

	virtual uint32_t get_count () const = 0;
	virtual boost::shared_ptr<Plugin> plugin (uint32_t num = 0) const = 0;
	virtual PluginType type () const = 0;

	enum UIElements : std::uint8_t {
		NoGUIToolbar  = 0x00,
		BypassEnable  = 0x01,
		PluginPreset  = 0x02,
		MIDIKeyboard  = 0x04,
		AllUIElements = 0x0f
	};

	virtual UIElements ui_elements () const = 0;

	virtual bool write_immediate_event (Evoral::EventType event_type, size_t size, const uint8_t* buf) = 0;
	virtual bool load_preset (Plugin::PresetRecord) = 0;

	virtual boost::shared_ptr<ReadOnlyControl> control_output (uint32_t) const = 0;

	virtual bool can_reset_all_parameters () = 0;
	virtual bool reset_parameters_to_default () = 0;

	virtual bool provides_stats () const = 0;
	virtual bool get_stats (PBD::microseconds_t&, PBD::microseconds_t&, double&, double&) const = 0;
	virtual void clear_stats () = 0;

protected:
	bool parse_plugin_type (XMLNode const&, PluginType&, std::string&) const;
	boost::shared_ptr<Plugin> find_and_load_plugin (Session&, XMLNode const&, PluginType&, std::string const&, bool& any_vst);

	void set_control_ids (const XMLNode&, int version);
	void preset_load_set_value (uint32_t, float);
};

} // namespace ARDOUR

#endif
