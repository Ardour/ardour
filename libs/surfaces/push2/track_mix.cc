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

#include "gtkmm2ext/rgb_macros.h"

#include "knob.h"
#include "menu.h"
#include "push2.h"
#include "track_mix.h"
#include "utils.h"

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

	Pango::FontDescription fd ("Sans Bold 14");
	name_layout->set_font_description (fd);

	Pango::FontDescription fd2 ("Sans 10");
	for (int n = 0; n < 8; ++n) {
		upper_layout[n] = Pango::Layout::create (context);
		upper_layout[n]->set_font_description (fd2);
		upper_layout[n]->set_text ("solo");
		lower_layout[n] = Pango::Layout::create (context);
		lower_layout[n]->set_font_description (fd2);
		lower_layout[n]->set_text ("mute");
	}

	Push2Knob* knob;

	knob = new Push2Knob (p2, context);
	knob->set_position (60, 80);
	knob->set_radius (35);
	knobs.push_back (knob);

	knob = new Push2Knob (p2, context);
	knob->set_position (180, 80);
	knob->set_radius (35);
	knobs.push_back (knob);
}

TrackMixLayout::~TrackMixLayout ()
{
	for (vector<Push2Knob*>::iterator k = knobs.begin(); k != knobs.end(); ++k) {
		delete *k;
	}
}

bool
TrackMixLayout::redraw (Cairo::RefPtr<Cairo::Context> context) const
{
	bool children_dirty = false;

	for (vector<Push2Knob*>::const_iterator k = knobs.begin(); k != knobs.end(); ++k) {
		if ((*k)->dirty()) {
			children_dirty = true;
			break;
		}
	}

	if (!children_dirty && !_dirty) {
		return false;
	}

	set_source_rgb (context, p2.get_color (Push2::DarkBackground));
	context->rectangle (0, 0, p2.cols, p2.rows);
	context->fill ();

	if (stripable) {
		int r,g,b,a;
		UINT_TO_RGBA (stripable->presentation_info().color(), &r, &g, &b, &a);
		context->set_source_rgb (r/255.0, g/255.0, b/255.0);
	} else {
		context->set_source_rgb (0.23, 0.0, 0.349);
	}
	context->move_to (10, 2);
	name_layout->update_from_cairo_context (context);
	name_layout->show_in_cairo_context (context);

	for (vector<Push2Knob*>::const_iterator k = knobs.begin(); k != knobs.end(); ++k) {
		(*k)->redraw (context);
	}

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
	if (!stripable) {
		return;
	}

	switch (n) {
	case 0: /* gain */
		boost::shared_ptr<AutomationControl> ac = stripable->gain_control();
		if (ac) {
			ac->set_value (ac->get_value() + ((2.0/64.0) * delta), PBD::Controllable::UseGroup);
		}
		break;
	}
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

		stripable->PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&TrackMixLayout::stripable_property_change, this, _1), &p2);
		stripable->presentation_info().PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&TrackMixLayout::stripable_property_change, this, _1), &p2);

		knobs[0]->set_controllable (stripable->gain_control());
		knobs[1]->set_controllable (stripable->pan_azimuth_control());

		name_changed ();
		color_changed ();
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

void
TrackMixLayout::color_changed ()
{
	uint32_t rgb = stripable->presentation_info().color();
	uint8_t index = p2.get_color_index (rgb);

	Push2::Button* b = p2.button_by_id (Push2::Upper1);
	b->set_color (index);
	b->set_state (Push2::LED::OneShot24th);
	p2.write (b->state_msg ());
}

void
TrackMixLayout::stripable_property_change (PropertyChange const& what_changed)
{
	if (what_changed.contains (Properties::color)) {
		color_changed ();
	}
	if (what_changed.contains (Properties::name)) {
		name_changed ();
	}
}
