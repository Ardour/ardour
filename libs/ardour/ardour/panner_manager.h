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

#ifndef __ardour_panner_manager_h__
#define __ardour_panner_manager_h__

#include <dlfcn.h>
#include "ardour/panner.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

struct PannerInfo {
	PanPluginDescriptor descriptor;
	void* module;

	PannerInfo (PanPluginDescriptor& d, void* handle)
	: descriptor (d)
	, module (handle)
	{}

	~PannerInfo () {
		dlclose (module);
	}
};

class PannerManager : public ARDOUR::SessionHandlePtr
{
public:
	~PannerManager ();
	static PannerManager& instance ();

	void discover_panners ();
	std::list<PannerInfo*> panner_info;

	PannerInfo* select_panner (ChanCount in, ChanCount out, std::string const uri = "");
	PannerInfo* get_by_uri (std::string uri);

private:
	PannerManager();
	static PannerManager* _instance;

	PannerInfo* get_descriptor (std::string path);
	int panner_discover (std::string path);
};

} // namespace

#endif /* __ardour_panner_manager_h__ */
