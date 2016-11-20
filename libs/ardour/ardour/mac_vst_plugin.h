/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2004 Paul Davis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __ardour_mac_vst_plugin_h__
#define __ardour_mac_vst_plugin_h__

#include "ardour/vst_plugin.h"

struct LIBARDOUR_API _VSTHandle;
typedef struct _VSTHandle VSTHandle;

namespace ARDOUR {

class AudioEngine;
class Session;

class LIBARDOUR_API MacVSTPlugin : public VSTPlugin
{
public:
	MacVSTPlugin (AudioEngine &, Session &, VSTHandle *, int unique_id);
	MacVSTPlugin (const MacVSTPlugin &);
	~MacVSTPlugin ();

	std::string state_node_name () const { return "mac-vst"; }
};

class LIBARDOUR_API MacVSTPluginInfo : public PluginInfo
{
public:
	MacVSTPluginInfo ();
	~MacVSTPluginInfo () {}

	PluginPtr load (Session& session);
	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;
};

} // namespace ARDOUR

#endif /* __ardour_mac_vst_plugin_h__ */
