/*
    Copyright (C) 2000 Paul Davis 

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

#include <string>

#include <sigc++/bind.h>

#include <pbd/failed_constructor.h>
#include <pbd/xml++.h>

#include <ardour/insert.h>
#include <ardour/plugin.h>
#include <ardour/port.h>
#include <ardour/route.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/buffer_set.h>

#ifdef VST_SUPPORT
#include <ardour/vst_plugin.h>
#endif

#ifdef HAVE_AUDIOUNITS
#include <ardour/audio_unit.h>
#endif

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/types.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Insert::Insert(Session& s, string name, Placement p)
	: Redirect (s, name, p)
	, _configured(false)
{
	// FIXME: default type?
}

Insert::Insert(Session& s, string name, Placement p, int imin, int imax, int omin, int omax)
	: Redirect (s, name, p, imin, imax, omin, omax)
	, _configured(false)
{
	// FIXME: default type?
}

