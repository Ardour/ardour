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
#include "ardour/automation_control.h"
#include "ardour/plugin.h"
#include "ardour/plugin_types.h"

namespace ARDOUR {

class Plugin;
class ReadOnlyControl;
class Route;
class Session;

class LIBARDOUR_API PlugInsertBase : virtual public Evoral::ControlSet, virtual public PBD::Destructible
{
public:
	virtual ~PlugInsertBase () {}

	virtual uint32_t get_count () const = 0;
	virtual std::shared_ptr<Plugin> plugin (uint32_t num = 0) const = 0;
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

	virtual std::shared_ptr<ReadOnlyControl> control_output (uint32_t) const = 0;

	virtual bool can_reset_all_parameters () = 0;
	virtual bool reset_parameters_to_default () = 0;

	virtual std::string describe_parameter (Evoral::Parameter param) = 0;

	virtual bool provides_stats () const = 0;
	virtual bool get_stats (PBD::microseconds_t&, PBD::microseconds_t&, double&, double&) const = 0;
	virtual void clear_stats () = 0;

	/** A control that manipulates a plugin parameter (control port). */
	struct PluginControl : public AutomationControl {
		PluginControl (Session&                        s,
		               PlugInsertBase*                 p,
		               const Evoral::Parameter&        param,
		               const ParameterDescriptor&      desc,
		               std::shared_ptr<AutomationList> list = std::shared_ptr<AutomationList> ());

		double      get_value (void) const;
		void        catch_up_with_external_value (double val);
		XMLNode&    get_state () const;
		std::string get_user_string () const;

	protected:
		virtual void    actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
		PlugInsertBase* _pib;
	};

	/** A control that manipulates a plugin property (message). */
	struct PluginPropertyControl : public AutomationControl {
		PluginPropertyControl (Session&                        s,
		                       PlugInsertBase*                 p,
		                       const Evoral::Parameter&        param,
		                       const ParameterDescriptor&      desc,
		                       std::shared_ptr<AutomationList> list = std::shared_ptr<AutomationList> ());

		double   get_value (void) const;
		XMLNode& get_state () const;

	protected:
		virtual void    actually_set_value (double value, PBD::Controllable::GroupControlDisposition);
		PlugInsertBase* _pib;
		Variant         _value;
	};

protected:
	static std::shared_ptr<Plugin> plugin_factory (std::shared_ptr<Plugin>);

	bool parse_plugin_type (XMLNode const&, PluginType&, std::string&) const;
	std::shared_ptr<Plugin> find_and_load_plugin (Session&, XMLNode const&, PluginType&, std::string const&, bool& any_vst);

	void set_control_ids (const XMLNode&, int version);
	void preset_load_set_value (uint32_t, float);
};

} // namespace ARDOUR

#endif
