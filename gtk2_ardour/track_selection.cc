/*
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#include <algorithm>

#include "ardour/route_group.h"
#include "control_protocol/control_protocol.h"

#include "track_selection.h"
#include "time_axis_view.h"
#include "public_editor.h"
#include "vca_time_axis.h"

using namespace std;

TrackSelection::TrackSelection (PublicEditor const * e, TrackViewList const &t)
	: TrackViewList (t)
	, _editor (e)
{
}

TrackSelection::~TrackSelection ()
{
}

