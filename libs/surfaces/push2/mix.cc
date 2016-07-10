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

#include "push2.h"
#include "mix.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

MixLayout::MixLayout (Push2& p, Session& s, Cairo::RefPtr<Cairo::Context> context)
	: Push2Layout (p, s)
	, bank_start (0)
{
	tc_clock_layout = Pango::Layout::create (context);
	bbt_clock_layout = Pango::Layout::create (context);

	Pango::FontDescription fd ("Sans Bold 24");
	tc_clock_layout->set_font_description (fd);
	bbt_clock_layout->set_font_description (fd);

	Pango::FontDescription fd2 ("Sans 10");
	for (int n = 0; n < 8; ++n) {
		upper_layout[n] = Pango::Layout::create (context);
		upper_layout[n]->set_font_description (fd2);
		upper_layout[n]->set_text ("solo");
		lower_layout[n] = Pango::Layout::create (context);
		lower_layout[n]->set_font_description (fd2);
		lower_layout[n]->set_text ("mute");
	}

	Pango::FontDescription fd3 ("Sans Bold 10");
	for (int n = 0; n < 8; ++n) {
		mid_layout[n] = Pango::Layout::create (context);
		mid_layout[n]->set_font_description (fd3);
	}

	switch_bank (0);
}

MixLayout::~MixLayout ()
{
}

bool
MixLayout::redraw (Cairo::RefPtr<Cairo::Context> context) const
{
	framepos_t audible = session.audible_frame();
	Timecode::Time TC;
	bool negative = false;
	string tc_clock_text;
	string bbt_clock_text;

	if (audible < 0) {
		audible = -audible;
		negative = true;
	}

	session.timecode_time (audible, TC);

	TC.negative = TC.negative || negative;

	tc_clock_text = Timecode::timecode_format_time(TC);

	Timecode::BBT_Time bbt = session.tempo_map().bbt_at_frame (audible);
	char buf[16];

#define BBT_BAR_CHAR "|"

	if (negative) {
		snprintf (buf, sizeof (buf), "-%03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
		          bbt.bars, bbt.beats, bbt.ticks);
	} else {
		snprintf (buf, sizeof (buf), " %03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
		          bbt.bars, bbt.beats, bbt.ticks);
	}

	bbt_clock_text = buf;

	bool dirty = false;

	if (tc_clock_text != tc_clock_layout->get_text()) {
		dirty = true;
		tc_clock_layout->set_text (tc_clock_text);
	}

	if (bbt_clock_text != tc_clock_layout->get_text()) {
		dirty = true;
		bbt_clock_layout->set_text (bbt_clock_text);
	}

	string mid_text;

	for (int n = 0; n < 8; ++n) {
		if (stripable[n]) {
			mid_text = short_version (stripable[n]->name(), 10);
			if (mid_text != mid_layout[n]->get_text()) {
				mid_layout[n]->set_text (mid_text);
				dirty = true;
			}
		}
	}

	if (!dirty) {
		return false;
	}

	context->set_source_rgb (0.764, 0.882, 0.882);
	context->rectangle (0, 0, 960, 160);
	context->fill ();

	/* clocks */

	context->set_source_rgb (0.23, 0.0, 0.349);
	context->move_to (650, 25);
	tc_clock_layout->update_from_cairo_context (context);
	tc_clock_layout->show_in_cairo_context (context);
	context->move_to (650, 60);
	bbt_clock_layout->update_from_cairo_context (context);
	bbt_clock_layout->show_in_cairo_context (context);

	for (int n = 0; n < 8; ++n) {
		context->move_to (10 + (n*120), 2);
		upper_layout[n]->update_from_cairo_context (context);
		upper_layout[n]->show_in_cairo_context (context);
	}

	for (int n = 0; n < 8; ++n) {
		context->move_to (10 + (n*120), 140);
		lower_layout[n]->update_from_cairo_context (context);
		lower_layout[n]->show_in_cairo_context (context);
	}

	for (int n = 0; n < 8; ++n) {
		if (stripable[n] && stripable[n]->presentation_info().selected()) {
			context->rectangle (10 + (n*120) - 5, 115, 120, 22);
			context->set_source_rgb (1.0, 0.737, 0.172);
			context->fill();
		}
		context->set_source_rgb (0.0, 0.0, 0.0);
		context->move_to (10 + (n*120), 120);
		mid_layout[n]->update_from_cairo_context (context);
		mid_layout[n]->show_in_cairo_context (context);
	}

	return true;
}

void
MixLayout::button_upper (uint32_t n)
{
	if (!stripable[n]) {
		return;
	}

	if (p2.modifier_state() & Push2::ModShift) {
		boost::shared_ptr<AutomationControl> sc = stripable[n]->rec_enable_control ();
		if (sc) {
			sc->set_value (!sc->get_value(), PBD::Controllable::UseGroup);
		}
	} else {
		boost::shared_ptr<SoloControl> sc = stripable[n]->solo_control ();
		if (sc) {
			sc->set_value (!sc->self_soloed(), PBD::Controllable::UseGroup);
		}
	}
}

void
MixLayout::button_lower (uint32_t n)
{
	if (!stripable[n]) {
		return;
	}

	if (p2.modifier_state() & Push2::ModSelect) {
		ControlProtocol::SetStripableSelection (stripable[n]);
	} else {
		boost::shared_ptr<MuteControl> mc = stripable[n]->mute_control ();

		if (mc) {
			mc->set_value (!mc->muted_by_self(), PBD::Controllable::UseGroup);
		}
	}
}

void
MixLayout::strip_vpot (int n, int delta)
{
	if (stripable[n]) {
		boost::shared_ptr<AutomationControl> ac = stripable[n]->gain_control();
		if (ac) {
			ac->set_value (ac->get_value() + ((2.0/64.0) * delta), PBD::Controllable::UseGroup);
		}
	}
}

void
MixLayout::strip_vpot_touch (int n, bool touching)
{
	if (stripable[n]) {
		boost::shared_ptr<AutomationControl> ac = stripable[n]->gain_control();
		if (ac) {
			if (touching) {
				ac->start_touch (session.audible_frame());
			} else {
				ac->stop_touch (true, session.audible_frame());
			}
		}
	}
}

void
MixLayout::stripable_property_change (PropertyChange const& what_changed, int which)
{
	if (what_changed.contains (Properties::selected)) {
		if (!stripable[which]) {
			return;
		}

		/* cancel string, which will cause a redraw on the next update
		 * cycle. The redraw will reflect selected status
		 */

		mid_layout[which]->set_text (string());
	}
}


void
MixLayout::solo_change (int n)
{
	Push2::ButtonID bid;

	switch (n) {
	case 0:
		bid = Push2::Upper1;
		break;
	case 1:
		bid = Push2::Upper2;
		break;
	case 2:
		bid = Push2::Upper3;
		break;
	case 3:
		bid = Push2::Upper4;
		break;
	case 4:
		bid = Push2::Upper5;
		break;
	case 5:
		bid = Push2::Upper6;
		break;
	case 6:
		bid = Push2::Upper7;
		break;
	case 7:
		bid = Push2::Upper8;
		break;
	default:
		return;
	}

	boost::shared_ptr<SoloControl> ac = stripable[n]->solo_control ();
	if (!ac) {
		return;
	}

	Push2::Button* b = p2.button_by_id (bid);

	if (ac->soloed()) {
		b->set_color (Push2::LED::Green);
	} else {
		b->set_color (Push2::LED::Black);
	}

	if (ac->soloed_by_others_upstream() || ac->soloed_by_others_downstream()) {
		b->set_state (Push2::LED::Blinking4th);
	} else {
		b->set_state (Push2::LED::OneShot24th);
	}

	p2.write (b->state_msg());
}

void
MixLayout::mute_change (int n)
{
	Push2::ButtonID bid;

	if (!stripable[n]) {
		return;
	}

	cerr << "Mute changed on " << n << ' ' << stripable[n]->name() << endl;

	switch (n) {
	case 0:
		bid = Push2::Lower1;
		break;
	case 1:
		bid = Push2::Lower2;
		break;
	case 2:
		bid = Push2::Lower3;
		break;
	case 3:
		bid = Push2::Lower4;
		break;
	case 4:
		bid = Push2::Lower5;
		break;
	case 5:
		bid = Push2::Lower6;
		break;
	case 6:
		bid = Push2::Lower7;
		break;
	case 7:
		bid = Push2::Lower8;
		break;
	default:
		return;
	}

	boost::shared_ptr<MuteControl> mc = stripable[n]->mute_control ();

	if (!mc) {
		return;
	}

	Push2::Button* b = p2.button_by_id (bid);

	if (Config->get_show_solo_mutes() && !Config->get_solo_control_is_listen_control ()) {

		if (mc->muted_by_self ()) {
			/* full mute */
			b->set_color (Push2::LED::Blue);
			b->set_state (Push2::LED::OneShot24th);
			cerr << "FULL MUTE1\n";
		} else if (mc->muted_by_others_soloing () || mc->muted_by_masters ()) {
			/* this will reflect both solo mutes AND master mutes */
			b->set_color (Push2::LED::Blue);
			b->set_state (Push2::LED::Blinking4th);
			cerr << "OTHER MUTE1\n";
		} else {
			/* no mute at all */
			b->set_color (Push2::LED::Black);
			b->set_state (Push2::LED::OneShot24th);
			cerr << "NO MUTE1\n";
		}

	} else {

		if (mc->muted_by_self()) {
			/* full mute */
			b->set_color (Push2::LED::Blue);
			b->set_state (Push2::LED::OneShot24th);
			cerr << "FULL MUTE2\n";
		} else if (mc->muted_by_masters ()) {
			/* this shows only master mutes, not mute-by-others-soloing */
			b->set_color (Push2::LED::Blue);
			b->set_state (Push2::LED::Blinking4th);
			cerr << "OTHER MUTE1\n";
		} else {
			/* no mute at all */
			b->set_color (Push2::LED::Black);
			b->set_state (Push2::LED::OneShot24th);
			cerr << "NO MUTE2\n";
		}
	}

	p2.write (b->state_msg());
}

void
MixLayout::switch_bank (uint32_t base)
{
	stripable_connections.drop_connections ();

	/* try to get the first stripable for the requested bank */

	stripable[0] = session.get_remote_nth_stripable (base, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));

	if (!stripable[0]) {
		return;
	}

	/* at least one stripable in this bank */
	bank_start = base;

	stripable[1] = session.get_remote_nth_stripable (base+1, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[2] = session.get_remote_nth_stripable (base+2, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[3] = session.get_remote_nth_stripable (base+3, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[4] = session.get_remote_nth_stripable (base+4, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[5] = session.get_remote_nth_stripable (base+5, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[6] = session.get_remote_nth_stripable (base+6, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
	stripable[7] = session.get_remote_nth_stripable (base+7, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));


	for (int n = 0; n < 8; ++n) {
		if (!stripable[n]) {
			continue;
		}

		/* stripable goes away? refill the bank, starting at the same point */

		stripable[n]->DropReferences.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&MixLayout::switch_bank, this, bank_start), &p2);
		boost::shared_ptr<AutomationControl> sc = stripable[n]->solo_control();
		if (sc) {
			sc->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&MixLayout::solo_change, this, n), &p2);
		}

		boost::shared_ptr<AutomationControl> mc = stripable[n]->mute_control();
		if (mc) {
			mc->Changed.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&MixLayout::mute_change, this, n), &p2);
		}

		stripable[n]->presentation_info().PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&MixLayout::stripable_property_change, this, _1, n), &p2);

		solo_change (n);
		mute_change (n);

	}
}

void
MixLayout::button_right ()
{
	switch_bank (max (0, bank_start + 8));
}

void
MixLayout::button_left ()
{
	switch_bank (max (0, bank_start - 8));
}

void
MixLayout::button_select_press ()
{
}

void
MixLayout::button_select_release ()
{
	if (!(p2.modifier_state() & Push2::ModSelect)) {
		/* somebody else used us as a modifier */
		return;
	}

	int selected = -1;

	for (int n = 0; n < 8; ++n) {
		if (stripable[n]) {
			if (stripable[n]->presentation_info().selected()) {
					selected = n;
					break;
			}
		}
	}

	if (selected < 0) {

		/* no visible track selected, select first (if any) */

		if (stripable[0]) {
			ControlProtocol::SetStripableSelection (stripable[0]);
		}

	} else {

		if (p2.modifier_state() & Push2::ModShift) {
			std::cerr << "select prev\n";
			/* select prev */

			if (selected == 0) {
				/* current selected is leftmost ... cancel selection,
				   switch banks by one, and select leftmost
				*/
				if (bank_start != 0) {
					ControlProtocol::ClearStripableSelection ();
					switch_bank (bank_start-1);
					if (stripable[0]) {
						ControlProtocol::SetStripableSelection (stripable[0]);
					}
				}
			} else {
				/* select prev, if any */
				int n = selected - 1;
				while (n >= 0 && !stripable[n]) {
					--n;
				}
				if (n >= 0) {
					ControlProtocol::SetStripableSelection (stripable[n]);
				}
			}

		} else {

			std::cerr << "select next\n";
			/* select next */

			if (selected == 7) {
				/* current selected is rightmost ... cancel selection,
				   switch banks by one, and select righmost
				*/
				ControlProtocol::ToggleStripableSelection (stripable[selected]);
				switch_bank (bank_start+1);
				if (stripable[7]) {
					ControlProtocol::SetStripableSelection (stripable[7]);
				}
			} else {
				/* select next, if any */
				int n = selected + 1;
				while (n < 8 && !stripable[n]) {
					++n;
				}

				if (n != 8) {
					ControlProtocol::SetStripableSelection (stripable[n]);
				}
			}
		}
	}
}


