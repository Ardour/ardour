/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_windows_vst_plugin_h__
#define __ardour_windows_vst_plugin_h__

#include "ardour/vst_plugin.h"

struct LIBARDOUR_API _VSTHandle;
typedef struct _VSTHandle VSTHandle;

namespace ARDOUR {

class AudioEngine;
class Session;
struct VST2Info;

class LIBARDOUR_API WindowsVSTPlugin : public VSTPlugin
{
public:
	WindowsVSTPlugin (AudioEngine &, Session &, VSTHandle *, int unique_id);
	WindowsVSTPlugin (const WindowsVSTPlugin &);
	~WindowsVSTPlugin ();

	std::string state_node_name () const { return "windows-vst"; }
};

class LIBARDOUR_API WindowsVSTPluginInfo : public VSTPluginInfo
{
public:
	WindowsVSTPluginInfo (VST2Info const&);
	~WindowsVSTPluginInfo () {}

	PluginPtr load (Session& session);
	std::vector<Plugin::PresetRecord> get_presets (bool user_only) const;
};

} // namespace ARDOUR

#endif /* __ardour_vst_plugin_h__ */
