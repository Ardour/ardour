/*
    Copyright (C) 2004 Paul Davis

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

#ifndef __ardour_lxvst_plugin_h__
#define __ardour_lxvst_plugin_h__

#include "ardour/vst_plugin.h"

struct LIBARDOUR_API _VSTHandle;
typedef struct _VSTHandle VSTHandle;

namespace ARDOUR {

class AudioEngine;
class Session;

class LIBARDOUR_API LXVSTPlugin : public VSTPlugin
{
  public:
	LXVSTPlugin (AudioEngine &, Session &, VSTHandle *);
	LXVSTPlugin (const LXVSTPlugin &);
	~LXVSTPlugin ();

	std::string state_node_name () const { return "lxvst"; }
};

class LIBARDOUR_API LXVSTPluginInfo : public PluginInfo
{
  public:
	LXVSTPluginInfo ();
	~LXVSTPluginInfo () {}

	PluginPtr load (Session& session);
};

} // namespace ARDOUR

#endif /* __ardour_lxvst_plugin_h__ */
