/*
    Copyright (C) 2004-2007 Paul Davis

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

#include <algorithm>
#include <cmath>

#include "pbd/error.h"

#include "ardour/types.h"
#include "ardour/pitch.h"
#include "ardour/audiofilesource.h"
#include "ardour/session.h"
#include "ardour/audioregion.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Pitch::Pitch (Session& s, TimeFXRequest& req)
	: Filter (s)
	, tsr (req)

{

}

int
Pitch::run (boost::shared_ptr<Region> region)
{
	tsr.done = true;

	return 1;
}
