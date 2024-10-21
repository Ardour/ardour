/*
 * Copyright (C) 2016-2023 Robin Gareus <robin@gareus.org>
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

#pragma once

#include "ardour/plugin_insert.h"
#include "ardour/route.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "ardour_dialog.h"

class PluginSetupDialog : public ArdourDialog
{
public:
	PluginSetupDialog (std::shared_ptr<ARDOUR::Route>, std::shared_ptr<ARDOUR::PluginInsert>, ARDOUR::Route::PluginSetupOptions);

	bool fan_out () const { return _fan_out.get_active () && _fan_out.get_sensitive (); }

	static std::string preset_label (uint32_t);

private:
	void setup_output_presets ();
	void update_sensitivity (uint32_t);
	bool io_match () const;

	void select_output_preset (uint32_t n_audio);
	void apply_mapping ();
	void toggle_fan_out ();

	std::shared_ptr<ARDOUR::Route> _route;
	std::shared_ptr<ARDOUR::PluginInsert> _pi;

	ArdourWidgets::ArdourDropdown _out_presets;
	ArdourWidgets::ArdourButton _keep_mapping;
	ArdourWidgets::ArdourButton _fan_out;
	ARDOUR::ChanCount _cur_inputs;
	ARDOUR::ChanCount _cur_outputs;
};

