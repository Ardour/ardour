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

#include <cairomm/region.h>
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
#include "ardour/vca_manager.h"

#include "canvas/colors.h"
#include "canvas/rectangle.h"

#include "gtkmm2ext/gui_thread.h"

#include "canvas.h"
#include "mix.h"
#include "knob.h"
#include "push2.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace ArdourCanvas;

MixLayout::MixLayout (Push2& p, Session& s)
	: Push2Layout (p, s)
	, bank_start (0)
	, vpot_mode (Volume)
{
	selection_bg = new Rectangle (this);
	selection_bg->hide ();

	Pango::FontDescription fd2 ("Sans 10");
	for (int n = 0; n < 8; ++n) {

		/* background for text labels for knob function */

		Rectangle* r = new Rectangle (this);
		Coord x0 = 10 + (n*Push2Canvas::inter_button_spacing()) - 5;
		r->set (Rect (x0, 2, x0 + Push2Canvas::inter_button_spacing(), 2 + 21));
		backgrounds.push_back (r);

		/* text labels for knob function*/

		Text* t = new Text (this);
		t->set_font_description (fd2);
		t->set_color (p2.get_color (Push2::ParameterName));
		t->set_position (Duple (10 + (n*Push2Canvas::inter_button_spacing()), 5));

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
		t->set (txt);
		upper_text.push_back (t);

		/* knobs */

		knobs[n] = new Push2Knob (p2, this);
		knobs[n]->set_position (Duple (60 + (n*Push2Canvas::inter_button_spacing()), 95));
		knobs[n]->set_radius (25);

		/* stripable names */

		t = new Text (this);
		t->set_font_description (fd2);
		t->set_color (p2.get_color (Push2::ParameterName));
		t->set_position (Duple (10 + (n*Push2Canvas::inter_button_spacing()), 140));
		lower_text.push_back (t);

	}

	mode_button = p2.button_by_id (Push2::Upper1);

	session.RouteAdded.connect (session_connections, invalidator(*this), boost::bind (&MixLayout::stripables_added, this), &p2);
	session.vca_manager().VCAAdded.connect (session_connections, invalidator (*this), boost::bind (&MixLayout::stripables_added, this), &p2);
}

MixLayout::~MixLayout ()
{
	// Item destructor deletes all children
}

void
MixLayout::show ()
{
	Container::show ();

	mode_button->set_color (Push2::LED::White);
	mode_button->set_state (Push2::LED::OneShot24th);
	p2.write (mode_button->state_msg());

	switch_bank (bank_start);
}

void
MixLayout::render (Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	DEBUG_TRACE (DEBUG::Push2, string_compose ("mix render %1\n", area));

	/* draw background */

	set_source_rgb (context, p2.get_color (Push2::DarkBackground));
	context->rectangle (0, 0, display_width(), display_height());
	context->fill ();

	/* draw line across top (below labels) */

	context->move_to (0, 22.5);
	context->line_to (display_width(), 22.5);
	context->set_line_width (1.0);
	context->stroke ();

	/* show the kids ... */

	render_children (area, context);
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

	for (int s = 0; s < 8; ++s) {
		backgrounds[s]->hide ();
		upper_text[s]->set_color (p2.get_color (Push2::ParameterName));
	}

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
		backgrounds[0]->set_fill_color (p2.get_color (Push2::ParameterName));
		backgrounds[0]->show ();
		upper_text[0]->set_color (contrasting_text_color (p2.get_color (Push2::ParameterName)));
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
		backgrounds[1]->set_fill_color (p2.get_color (Push2::ParameterName));
		backgrounds[1]->show ();
		upper_text[1]->set_color (contrasting_text_color (p2.get_color (Push2::ParameterName)));
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
		backgrounds[2]->set_fill_color (p2.get_color (Push2::ParameterName));
		backgrounds[2]->show ();
		upper_text[2]->set_color (contrasting_text_color (p2.get_color (Push2::ParameterName)));
		break;
	case Send1:
		for (int s = 0; s < 8; ++s) {
			if (stripable[s]) {
				knobs[s]->set_controllable (stripable[s]->send_level_controllable (0));
			} else {
				knobs[s]->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			knobs[s]->remove_flag (Push2Knob::ArcToZero);
		}
		backgrounds[3]->set_fill_color (p2.get_color (Push2::ParameterName));
		backgrounds[3]->show ();
		upper_text[3]->set_color (contrasting_text_color (p2.get_color (Push2::ParameterName)));
		break;
	case Send2:
		for (int s = 0; s < 8; ++s) {
			if (stripable[s]) {
				knobs[s]->set_controllable (stripable[s]->send_level_controllable (1));
			} else {
				knobs[s]->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			knobs[s]->remove_flag (Push2Knob::ArcToZero);
		}
		backgrounds[4]->set_fill_color (p2.get_color (Push2::ParameterName));
		backgrounds[4]->show ();
		upper_text[4]->set_color (contrasting_text_color (p2.get_color (Push2::ParameterName)));
		break;
	case Send3:
		for (int s = 0; s < 8; ++s) {
			if (stripable[s]) {
				knobs[s]->set_controllable (stripable[s]->send_level_controllable (2));
			} else {
				knobs[s]->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			knobs[s]->remove_flag (Push2Knob::ArcToZero);
		}
		backgrounds[5]->set_fill_color (p2.get_color (Push2::ParameterName));
		backgrounds[5]->show ();
		upper_text[5]->set_color (contrasting_text_color (p2.get_color (Push2::ParameterName)));
		break;
	case Send4:
		for (int s = 0; s < 8; ++s) {
			if (stripable[s]) {
				knobs[s]->set_controllable (stripable[s]->send_level_controllable (3));
			} else {
				knobs[s]->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			knobs[s]->remove_flag (Push2Knob::ArcToZero);
		}
		backgrounds[6]->set_fill_color (p2.get_color (Push2::ParameterName));
		backgrounds[6]->show ();
		upper_text[6]->set_color (contrasting_text_color (p2.get_color (Push2::ParameterName)));
		break;
	case Send5:
		for (int s = 0; s < 8; ++s) {
			if (stripable[s]) {
				knobs[s]->set_controllable (stripable[s]->send_level_controllable (4));
			} else {
				knobs[s]->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			knobs[s]->remove_flag (Push2Knob::ArcToZero);
		}
		backgrounds[7]->set_fill_color (p2.get_color (Push2::ParameterName));
		backgrounds[7]->show ();
		upper_text[7]->set_color (contrasting_text_color (p2.get_color (Push2::ParameterName)));
		break;
	default:
		break;
	}
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
MixLayout::stripable_property_change (PropertyChange const& what_changed, uint32_t which)
{
	if (what_changed.contains (Properties::hidden)) {
		switch_bank (bank_start);
	}

	if (what_changed.contains (Properties::selected)) {

		if (!stripable[which]) {
			return;
		}

		if (stripable[which]->presentation_info().selected()) {
			show_selection (which);
		} else {
			hide_selection (which);
		}
	}

}

void
MixLayout::show_selection (uint32_t n)
{
	selection_bg->show ();
	selection_bg->set_fill_color (stripable[n]->presentation_info().color());
	const Coord x0 = 10  + (n * Push2Canvas::inter_button_spacing()) - 5;
	selection_bg->set (Rect (x0, 137, x0 + Push2Canvas::inter_button_spacing(), 137 + 21));
	lower_text[n]->set_color (ArdourCanvas::contrasting_text_color (selection_bg->fill_color()));
}

void
MixLayout::hide_selection (uint32_t n)
{
	selection_bg->hide ();
	if (stripable[n]) {
		lower_text[n]->set_color (stripable[n]->presentation_info().color());
	}
}

void
MixLayout::solo_changed (uint32_t n)
{
	solo_mute_changed (n);
}

void
MixLayout::mute_changed (uint32_t n)
{
	solo_mute_changed (n);
}

void
MixLayout::solo_mute_changed (uint32_t n)
{
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
	lower_text[n]->set (text);
}

void
MixLayout::switch_bank (uint32_t base)
{
	stripable_connections.drop_connections ();

	/* work backwards so we can tell if we should actually switch banks */

	boost::shared_ptr<Stripable> s[8];
	uint32_t different = 0;

	for (int n = 0; n < 8; ++n) {
		s[n] = session.get_remote_nth_stripable (base+n, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
		if (s[n] != stripable[n]) {
			different++;
		}
	}

	if (!different) {
		/* some missing strips; new bank the same or more empty stripables than the old one, do
		   nothing since we had already reached the end.
		*/
		return;
	}

	if (!s[0]) {
		/* not even the first stripable exists, do nothing */
		return;
	}

	for (int n = 0; n < 8; ++n) {
		stripable[n] = s[n];
	}

	/* at least one stripable in this bank */

	bank_start = base;

	for (int n = 0; n < 8; ++n) {
		if (!stripable[n]) {
			lower_text[n]->hide ();
			hide_selection (n);
			continue;
		}

		lower_text[n]->show ();

		/* stripable goes away? refill the bank, starting at the same point */

		stripable[n]->DropReferences.connect (stripable_connections, invalidator (*this), boost::bind (&MixLayout::switch_bank, this, bank_start), &p2);
		stripable[n]->presentation_info().PropertyChanged.connect (stripable_connections, invalidator (*this), boost::bind (&MixLayout::stripable_property_change, this, _1, n), &p2);
		stripable[n]->solo_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&MixLayout::solo_changed, this, n), &p2);
		stripable[n]->mute_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&MixLayout::mute_changed, this, n), &p2);

		if (stripable[n]->presentation_info().selected()) {
			show_selection (n);
		} else {
			hide_selection (n);
		}

		/* this will set lower text to the correct value (basically
		   the stripable name)
		*/

		solo_mute_changed (n);

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
		knobs[n]->set_arc_start_color (stripable[n]->presentation_info().color());
		knobs[n]->set_arc_end_color (stripable[n]->presentation_info().color());
	}

	show_vpot_mode ();
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

void
MixLayout::stripables_added ()
{
	/* reload current bank */
	switch_bank (bank_start);
}
