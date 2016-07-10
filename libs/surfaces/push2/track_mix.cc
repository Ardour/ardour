/*
  Copyright (C) 2016 Paul Davis

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/enumwriter.h"

#include "midi++/parser.h"
#include "timecode/time.h"
#include "timecode/bbt_time.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "menu.h"
#include "push2.h"
#include "track_mix.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

TrackMixLayout::TrackMixLayout (Push2& p, Session& s, Cairo::RefPtr<Cairo::Context> context)
	: Push2Layout (p, s)
	, _dirty (false)
{
	name_layout = Pango::Layout::create (context);

	Pango::FontDescription fd ("Sans Bold 24");
	name_layout->set_font_description (fd);
}

TrackMixLayout::~TrackMixLayout ()
{
}

bool
TrackMixLayout::redraw (Cairo::RefPtr<Cairo::Context> context) const
{
	if (!_dirty) {
		return false;
	}

	context->set_source_rgb (0.764, 0.882, 0.882);
	context->rectangle (0, 0, 960, 160);
	context->fill ();

	context->set_source_rgb (0.23, 0.0, 0.349);
	context->move_to (10, 2);
	name_layout->update_from_cairo_context (context);
	name_layout->show_in_cairo_context (context);

	return true;
}

void
TrackMixLayout::button_upper (uint32_t n)
{
}

void
TrackMixLayout::button_lower (uint32_t n)
{
}

void
TrackMixLayout::strip_vpot (int n, int delta)
{
}

void
TrackMixLayout::strip_vpot_touch (int, bool)
{
}

void
TrackMixLayout::set_stripable (boost::shared_ptr<Stripable> s)
{
	stripable = s;

	if (stripable) {
		stripable->DropReferences.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&TrackMixLayout::drop_stripable, this), &p2);
		name_changed ();
	}

	_dirty = true;
}

void
TrackMixLayout::drop_stripable ()
{
	stripable_connections.drop_connections ();
	stripable.reset ();
	_dirty = true;
}

void
TrackMixLayout::name_changed ()
{
	name_layout->set_text (stripable->name());
	_dirty = true;
}
