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

#include "canvas/colors.h"

#include "mix.h"
#include "knob.h"
#include "push2.h"
#include "utils.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

MixLayout::MixLayout (Push2& p, Session& s, Cairo::RefPtr<Cairo::Context> context)
	: Push2Layout (p, s)
	, _dirty (true)
	, bank_start (0)
	, vpot_mode (Volume)
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

		string txt;
		switch (n) {
		case 0:
			txt = _("Volumes");
			break;
		case 1:
			txt = _("Pans");
			break;
		case 2:
			txt = _("Pan Widths");
			break;
		case 3:
			txt = _("A Sends");
			break;
		case 4:
			txt = _("B Sends");
			break;
		case 5:
			txt = _("C Sends");
			break;
		case 6:
			txt = _("D Sends");
			break;
		case 7:
			txt = _("E Sends");
			break;
		}
		upper_layout[n]->set_text (txt);
	}

	Pango::FontDescription fd3 ("Sans 10");
	for (int n = 0; n < 8; ++n) {
		lower_layout[n] = Pango::Layout::create (context);
		lower_layout[n]->set_font_description (fd3);
	}

	for (int n = 0; n < 8; ++n) {
		knobs[n] = new Push2Knob (p2, context);
		knobs[n]->set_position (60 + (n*120), 95);
		knobs[n]->set_radius (25);
	}

	mode_button = p2.button_by_id (Push2::Upper1);
}

MixLayout::~MixLayout ()
{
	for (int n = 0; n < 8; ++n) {
		delete knobs[n];
	}
}

void
MixLayout::on_show ()
{
	mode_button->set_color (Push2::LED::White);
	mode_button->set_state (Push2::LED::OneShot24th);
	p2.write (mode_button->state_msg());

	switch_bank (bank_start);
	show_vpot_mode ();
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

	bool children_dirty = false;

	if (tc_clock_text != tc_clock_layout->get_text()) {
		children_dirty = true;
		tc_clock_layout->set_text (tc_clock_text);
	}

	if (bbt_clock_text != bbt_clock_layout->get_text()) {
		children_dirty = true;
		bbt_clock_layout->set_text (bbt_clock_text);
	}

	for (int n = 0; n < 8; ++n) {
		if (knobs[n]->dirty()) {
			children_dirty = true;
			break;
		}
	}

	for (int n = 0; n < 8; ++n) {
		if (stripable[n]) {
			string shortname = short_version (stripable[n]->name(), 10);
			string text;
			boost::shared_ptr<AutomationControl> ac;
			ac = stripable[n]->solo_control();
			if (ac && ac->get_value()) {
				text += "* ";
			}
			boost::shared_ptr<MuteControl> mc;
			mc = stripable[n]->mute_control ();
			if (mc) {
				if (mc->muted_by_self_or_masters()) {
					text += "! ";
				} else if (mc->muted_by_others_soloing()) {
					text += "- "; // it would be nice to use Unicode mute"\uD83D\uDD07 ";
				}
			}
			text += shortname;

			if (text != lower_layout[n]->get_text()) {
				lower_layout[n]->set_text (text);
				children_dirty = true;
			}
		}
	}

	if (!children_dirty && !_dirty) {
		return false;
	}

	set_source_rgb (context, p2.get_color (Push2::DarkBackground));
	context->rectangle (0, 0, p2.cols, p2.rows);
	context->fill ();

	/* clocks */

	set_source_rgb (context, ArdourCanvas::contrasting_text_color (p2.get_color (Push2::DarkBackground)));
	context->move_to (650, 30);
	tc_clock_layout->update_from_cairo_context (context);
	tc_clock_layout->show_in_cairo_context (context);
	context->move_to (650, 65);
	bbt_clock_layout->update_from_cairo_context (context);
	bbt_clock_layout->show_in_cairo_context (context);

	set_source_rgb (context, p2.get_color (Push2::ParameterName));

	for (int n = 0; n < 8; ++n) {

		if (upper_layout[n]->get_text().empty()) {
			continue;
		}

		/* Draw highlight box */

		uint32_t color = p2.get_color (Push2::ParameterName);

		if (n == (int) vpot_mode) {
			set_source_rgb (context, color);
			context->rectangle (10 + (n*120) - 5, 2, 120, 21);
			context->fill();
			set_source_rgb (context, ArdourCanvas::contrasting_text_color (color));
		}  else {
			set_source_rgb (context, color);
		}

		context->move_to (10 + (n*120), 2);
		upper_layout[n]->update_from_cairo_context (context);
		upper_layout[n]->show_in_cairo_context (context);
	}

	context->move_to (0, 22.5);
	context->line_to (p2.cols, 22.5);
	context->set_line_width (1.0);
	context->stroke ();

	for (int n = 0; n < 8; ++n) {
		knobs[n]->redraw (context);
	}

	for (int n = 0; n < 8; ++n) {

		if (lower_layout[n]->get_text().empty()) {
			continue;
		}

		if (stripable[n]) {
			uint32_t color = stripable[n]->presentation_info().color();

			if (stripable[n]->presentation_info().selected()) {
				set_source_rgb (context, color);
				context->rectangle (10 + (n*120) - 5, 137, 120, 21);
				context->fill();
				set_source_rgb (context, ArdourCanvas::contrasting_text_color (color));
			}  else {
				set_source_rgb (context, color);
			}

			context->move_to (10 + (n*120), 140);
			lower_layout[n]->update_from_cairo_context (context);
			lower_layout[n]->show_in_cairo_context (context);
		}
	}

	_dirty = false;

	return true;
}

void
MixLayout::button_upper (uint32_t n)
{
	Push2::Button* b;
	switch (n) {
	case 0:
		vpot_mode = Volume;
		b = p2.button_by_id (Push2::Upper1);
		break;
	case 1:
		vpot_mode = PanAzimuth;
		b = p2.button_by_id (Push2::Upper2);
		break;
	case 2:
		vpot_mode = PanWidth;
		b = p2.button_by_id (Push2::Upper3);
		break;
	case 3:
		vpot_mode = Send1;
		b = p2.button_by_id (Push2::Upper4);
		break;
	case 4:
		vpot_mode = Send2;
		b = p2.button_by_id (Push2::Upper5);
		break;
	case 5:
		vpot_mode = Send3;
		b = p2.button_by_id (Push2::Upper6);
		break;
	case 6:
		vpot_mode = Send4;
		b = p2.button_by_id (Push2::Upper7);
		break;
	case 7:
		vpot_mode = Send5;
		b = p2.button_by_id (Push2::Upper8);
		break;
	}

	if (b != mode_button) {
		mode_button->set_color (Push2::LED::Black);
		mode_button->set_state (Push2::LED::OneShot24th);
		p2.write (mode_button->state_msg());
	}

	mode_button = b;

	show_vpot_mode ();
}

void
MixLayout::show_vpot_mode ()
{
	mode_button->set_color (Push2::LED::White);
	mode_button->set_state (Push2::LED::OneShot24th);
	p2.write (mode_button->state_msg());

	boost::shared_ptr<AutomationControl> ac;
	switch (vpot_mode) {
	case Volume:
		for (int s = 0; s < 8; ++s) {
			if (stripable[s]) {
				knobs[s]->set_controllable (stripable[s]->gain_control());
			} else {
				knobs[s]->set_controllable (boost::shared_ptr<AutomationControl>());
			}
			knobs[s]->remove_flag (Push2Knob::ArcToZero);
		}
		break;
	case PanAzimuth:
		for (int s = 0; s < 8; ++s) {
			if (stripable[s]) {
				knobs[s]->set_controllable (stripable[s]->pan_azimuth_control());
				knobs[s]->add_flag (Push2Knob::ArcToZero);
			} else {
				knobs[s]->set_controllable (boost::shared_ptr<AutomationControl>());

			}
		}
		break;
	case PanWidth:
		for (int s = 0; s < 8; ++s) {
			if (stripable[s]) {
				knobs[s]->set_controllable (stripable[s]->pan_width_control());
			} else {
				knobs[s]->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			knobs[s]->remove_flag (Push2Knob::ArcToZero);
		}
		break;
	default:
		break;
	}

	_dirty = true;
}

void
MixLayout::button_mute ()
{
	boost::shared_ptr<Stripable> s = ControlProtocol::first_selected_stripable();
	if (s) {
		boost::shared_ptr<AutomationControl> ac = s->mute_control();
		if (ac) {
			ac->set_value (!ac->get_value(), PBD::Controllable::UseGroup);
		}
	}
}

void
MixLayout::button_solo ()
{
	boost::shared_ptr<Stripable> s = ControlProtocol::first_selected_stripable();
	if (s) {
		boost::shared_ptr<AutomationControl> ac = s->solo_control();
		if (ac) {
			ac->set_value (!ac->get_value(), PBD::Controllable::UseGroup);
		}
	}
}

void
MixLayout::button_lower (uint32_t n)
{
	if (!stripable[n]) {
		return;
	}

	ControlProtocol::SetStripableSelection (stripable[n]);
}

void
MixLayout::strip_vpot (int n, int delta)
{
	boost::shared_ptr<Controllable> ac = knobs[n]->controllable();

	if (ac) {
		ac->set_value (ac->get_value() + ((2.0/64.0) * delta), PBD::Controllable::UseGroup);
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

		lower_layout[which]->set_text (string());
	}
}

void
MixLayout::solo_change (int n)
{
}

void
MixLayout::mute_change (int n)
{
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
		stripable[n]->presentation_info().PropertyChanged.connect (stripable_connections, MISSING_INVALIDATOR, boost::bind (&MixLayout::stripable_property_change, this, _1, n), &p2);

		solo_change (n);
		mute_change (n);

		Push2::Button* b;

		switch (n) {
		case 0:
			b = p2.button_by_id (Push2::Lower1);
			break;
		case 1:
			b = p2.button_by_id (Push2::Lower2);
			break;
		case 2:
			b = p2.button_by_id (Push2::Lower3);
			break;
		case 3:
			b = p2.button_by_id (Push2::Lower4);
			break;
		case 4:
			b = p2.button_by_id (Push2::Lower5);
			break;
		case 5:
			b = p2.button_by_id (Push2::Lower6);
			break;
		case 6:
			b = p2.button_by_id (Push2::Lower7);
			break;
		case 7:
			b = p2.button_by_id (Push2::Lower8);
			break;
		}

		b->set_color (p2.get_color_index (stripable[n]->presentation_info().color()));
		b->set_state (Push2::LED::OneShot24th);
		p2.write (b->state_msg());

		knobs[n]->set_text_color (stripable[n]->presentation_info().color());
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
