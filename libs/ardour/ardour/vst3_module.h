/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_vst3_module_h_
#define _ardour_vst3_module_h_

#include <boost/shared_ptr.hpp>

#include "ardour/libardour_visibility.h"

namespace Steinberg {
	class IPluginFactory;
}

namespace ARDOUR {

class LIBARDOUR_API VST3PluginModule
{
public:
	static boost::shared_ptr<VST3PluginModule> load (std::string const& path);

	VST3PluginModule () : _factory (0) {}
	virtual ~VST3PluginModule () {}

	Steinberg::IPluginFactory* factory ();

protected:
	void release_factory ();

	virtual bool init () = 0;
	virtual bool exit () = 0;
	virtual void* fn_ptr (const char* name) const = 0;

private:
	/* prevent copy construction */
	VST3PluginModule (VST3PluginModule const&);

	Steinberg::IPluginFactory* _factory;
};

} // namespace ARDOUR
#endif
