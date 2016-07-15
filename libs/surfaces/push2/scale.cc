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
#include "scale.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

ScaleLayout::ScaleLayout (Push2& p, Session& s, Cairo::RefPtr<Cairo::Context> context)
	: Push2Layout (p, s)
{
	build_scale_menu (context);
}

ScaleLayout::~ScaleLayout ()
{
}

bool
ScaleLayout::redraw (Cairo::RefPtr<Cairo::Context> context, bool force) const
{
	bool draw = false;

	if (scale_menu->dirty()) {
		draw = true;
	}

	if (!draw) {
		return false;
	}

	context->set_source_rgb (0.764, 0.882, 0.882);
	context->rectangle (0, 0, 960, 160);
	context->fill ();

	scale_menu->redraw (context, force);

	return true;
}

void
ScaleLayout::button_upper (uint32_t n)
{
}

void
ScaleLayout::button_lower (uint32_t n)
{
}

void
ScaleLayout::strip_vpot (int n, int delta)
{
	if (n == 0) {
		scale_menu->step_active (n, delta);
		return;
	}
}

void
ScaleLayout::strip_vpot_touch (int, bool)
{
}

void
ScaleLayout::build_scale_menu (Cairo::RefPtr<Cairo::Context> context)
{
	vector<string> v;

	scale_menu = new Push2Menu (context);

	v.push_back ("Dorian");
	v.push_back ("IonianMajor");
	v.push_back ("Minor");
	v.push_back ("HarmonicMinor");
	v.push_back ("MelodicMinorAscending");
	v.push_back ("MelodicMinorDescending");
	v.push_back ("Phrygian");
	v.push_back ("Lydian");
	v.push_back ("Mixolydian");
	v.push_back ("Aeolian");
	v.push_back ("Locrian");
	v.push_back ("PentatonicMajor");
	v.push_back ("PentatonicMinor");
	v.push_back ("Chromatic");
	v.push_back ("BluesScale");
	v.push_back ("NeapolitanMinor");
	v.push_back ("NeapolitanMajor");
	v.push_back ("Oriental");
	v.push_back ("DoubleHarmonic");
	v.push_back ("Enigmatic");
	v.push_back ("Hirajoshi");
	v.push_back ("HungarianMinor");
	v.push_back ("HungarianMajor");
	v.push_back ("Kumoi");
	v.push_back ("Iwato");
	v.push_back ("Hindu");
	v.push_back ("Spanish8Tone");
	v.push_back ("Pelog");
	v.push_back ("HungarianGypsy");
	v.push_back ("Overtone");
	v.push_back ("LeadingWholeTone");
	v.push_back ("Arabian");
	v.push_back ("Balinese");
	v.push_back ("Gypsy");
	v.push_back ("Mohammedan");
	v.push_back ("Javanese");
	v.push_back ("Persian");
	v.push_back ("Algeria");

	scale_menu->fill_column (0, v);

	v.clear ();
}
