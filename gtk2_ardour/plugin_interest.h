/*
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#ifndef __gtkardour_plugin_interest_h__
#define __gtkardour_plugin_interest_h__

#include <vector>
#include "ardour/plugin.h"

typedef std::vector<ARDOUR::PluginPtr> SelectedPlugins;

class PluginInterestedObject
{
public:
	PluginInterestedObject() {}
	virtual ~PluginInterestedObject() {}

	virtual bool use_plugins (const SelectedPlugins&) = 0;
};

#endif /* __gtkardour_plugin_interest_h__ */
