/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#include "temporal/time.h"
#include "temporal/bbt_time.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/colors.h"
#include "canvas/line.h"
#include "canvas/rectangle.h"
#include "canvas/text.h"

#include "gtkmm2ext/gui_thread.h"

#include "canvas.h"
#include "knob.h"
#include "level_meter.h"
#include "mix.h"
#include "push2.h"
#include "utils.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace Gtkmm2ext;
using namespace ArdourCanvas;

MixLayout::MixLayout (Push2& p, Session & s, std::string const & name)
	: Push2Layout (p, s, name)
	, _bank_start (0)
	, _vpot_mode (Volume)
{
	/* background */

	_bg = new ArdourCanvas::Rectangle (this);
	_bg->set (Rect (0, 0, display_width(), display_height()));
	_bg->set_fill_color (_p2.get_color (Push2::DarkBackground));

	/* upper line */

	_upper_line = new Line (this);
	_upper_line->set (Duple (0, 22.5), Duple (display_width(), 22.5));
	_upper_line->set_outline_color (_p2.get_color (Push2::LightBackground));

	Pango::FontDescription fd2 ("Sans 10");

	for (int n = 0; n < 8; ++n) {

		/* background for text labels for knob function */

		ArdourCanvas::Rectangle* r = new ArdourCanvas::Rectangle (this);
		Coord x0 = 10 + (n*Push2Canvas::inter_button_spacing()) - 5;
		r->set (Rect (x0, 2, x0 + Push2Canvas::inter_button_spacing(), 2 + 21));
		_upper_backgrounds.push_back (r);

		r = new ArdourCanvas::Rectangle (this);
		r->set (Rect (x0, 137, x0 + Push2Canvas::inter_button_spacing(), 137 + 21));
		_lower_backgrounds.push_back (r);

		/* text labels for knob function*/

		Text* t = new Text (this);
		t->set_font_description (fd2);
		t->set_color (_p2.get_color (Push2::ParameterName));
		t->set_position (Duple (10 + (n*Push2Canvas::inter_button_spacing()), 5));

		std::string txt;
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
		_upper_text.push_back (t);

		/* GainMeters */

		gain_meter[n] = new GainMeter (this, _p2);
		gain_meter[n]->set_position (Duple (40 + (n * Push2Canvas::inter_button_spacing()), 95));

		/* stripable names */

		t = new Text (this);
		t->set_font_description (fd2);
		t->set_color (_p2.get_color (Push2::ParameterName));
		t->set_position (Duple (10 + (n*Push2Canvas::inter_button_spacing()), 140));
		_lower_text.push_back (t);

	}

	_mode_button = _p2.button_by_id (Push2::Upper1);

	_session.RouteAdded.connect (_session_connections, invalidator(*this), boost::bind (&MixLayout::stripables_added, this), &_p2);
	_session.vca_manager().VCAAdded.connect (_session_connections, invalidator (*this), boost::bind (&MixLayout::stripables_added, this), &_p2);
}

MixLayout::~MixLayout ()
{
	// Item destructor deletes all children
}

void
MixLayout::show ()
{
	Push2::ButtonID upper_buttons[] = { Push2::Upper1, Push2::Upper2, Push2::Upper3, Push2::Upper4,
	                                    Push2::Upper5, Push2::Upper6, Push2::Upper7, Push2::Upper8 };


	for (size_t n = 0; n < sizeof (upper_buttons) / sizeof (upper_buttons[0]); ++n) {
		boost::shared_ptr<Push2::Button> b = _p2.button_by_id (upper_buttons[n]);

		if (b != _mode_button) {
			b->set_color (Push2::LED::DarkGray);
		} else {
			b->set_color (Push2::LED::White);
		}
		b->set_state (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}

	switch_bank (_bank_start);

	Container::show ();
}

void
MixLayout::render (Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	Container::render (area, context);
}

void
MixLayout::button_upper (uint32_t n)
{
	boost::shared_ptr<Push2::Button> b;
	switch (n) {
	case 0:
		_vpot_mode = Volume;
		b = _p2.button_by_id (Push2::Upper1);
		break;
	case 1:
		_vpot_mode = PanAzimuth;
		b = _p2.button_by_id (Push2::Upper2);
		break;
	case 2:
		_vpot_mode = PanWidth;
		b = _p2.button_by_id (Push2::Upper3);
		break;
	case 3:
		_vpot_mode = Send1;
		b = _p2.button_by_id (Push2::Upper4);
		break;
	case 4:
		_vpot_mode = Send2;
		b = _p2.button_by_id (Push2::Upper5);
		break;
	case 5:
		_vpot_mode = Send3;
		b = _p2.button_by_id (Push2::Upper6);
		break;
	case 6:
		_vpot_mode = Send4;
		b = _p2.button_by_id (Push2::Upper7);
		break;
	case 7:
		_vpot_mode = Send5;
		b = _p2.button_by_id (Push2::Upper8);
		break;
	}

	if (b != _mode_button) {
		_mode_button->set_color (Push2::LED::Black);
		_mode_button->set_state (Push2::LED::OneShot24th);
		_p2.write (_mode_button->state_msg());
	}

	_mode_button = b;

	show_vpot_mode ();
}

void
MixLayout::show_vpot_mode ()
{
	_mode_button->set_color (Push2::LED::White);
	_mode_button->set_state (Push2::LED::OneShot24th);
	_p2.write (_mode_button->state_msg());

	for (int s = 0; s < 8; ++s) {
		_upper_backgrounds[s]->hide ();
		_upper_text[s]->set_color (_p2.get_color (Push2::ParameterName));
	}

	uint32_t n = 0;

	boost::shared_ptr<AutomationControl> ac;
	switch (_vpot_mode) {
	case Volume:
		for (int s = 0; s < 8; ++s) {
			if (_stripable[s]) {
				gain_meter[s]->knob->set_controllable (_stripable[s]->gain_control());
				boost::shared_ptr<PeakMeter> pm = _stripable[s]->peak_meter();
				if (pm) {
					gain_meter[s]->meter->set_meter (pm.get());
				} else {
					gain_meter[s]->meter->set_meter (0);
				}
			} else {
				gain_meter[s]->knob->set_controllable (boost::shared_ptr<AutomationControl>());
				gain_meter[s]->meter->set_meter (0);
			}
			gain_meter[s]->knob->remove_flag (Push2Knob::ArcToZero);
			gain_meter[s]->meter->show ();
		}
		n = 0;
		break;
	case PanAzimuth:
		for (int s = 0; s < 8; ++s) {
			if (_stripable[s]) {
				gain_meter[s]->knob->set_controllable (_stripable[s]->pan_azimuth_control());
				gain_meter[s]->knob->add_flag (Push2Knob::ArcToZero);
			} else {
				gain_meter[s]->knob->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			gain_meter[s]->meter->hide ();
		}
		n = 1;
		break;
	case PanWidth:
		for (int s = 0; s < 8; ++s) {
			if (_stripable[s]) {
				gain_meter[s]->knob->set_controllable (_stripable[s]->pan_width_control());
			} else {
				gain_meter[s]->knob->set_controllable (boost::shared_ptr<AutomationControl>());
			}
			gain_meter[s]->knob->remove_flag (Push2Knob::ArcToZero);
			gain_meter[s]->meter->hide ();
		}
		n = 2;
		break;
	case Send1:
		for (int s = 0; s < 8; ++s) {
			if (_stripable[s]) {
				gain_meter[s]->knob->set_controllable (_stripable[s]->send_level_controllable (0));
			} else {
				gain_meter[s]->knob->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			gain_meter[s]->knob->remove_flag (Push2Knob::ArcToZero);
			gain_meter[s]->meter->hide ();
		}
		n = 3;
		break;
	case Send2:
		for (int s = 0; s < 8; ++s) {
			if (_stripable[s]) {
				gain_meter[s]->knob->set_controllable (_stripable[s]->send_level_controllable (1));
			} else {
				gain_meter[s]->knob->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			gain_meter[s]->knob->remove_flag (Push2Knob::ArcToZero);
			gain_meter[s]->meter->hide ();
		}
		n = 4;
		break;
	case Send3:
		for (int s = 0; s < 8; ++s) {
			if (_stripable[s]) {
				gain_meter[s]->knob->set_controllable (_stripable[s]->send_level_controllable (2));
			} else {
				gain_meter[s]->knob->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			gain_meter[s]->knob->remove_flag (Push2Knob::ArcToZero);
			gain_meter[s]->meter->hide ();
		}
		n = 5;
		break;
	case Send4:
		for (int s = 0; s < 8; ++s) {
			if (_stripable[s]) {
				gain_meter[s]->knob->set_controllable (_stripable[s]->send_level_controllable (3));
			} else {
				gain_meter[s]->knob->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			gain_meter[s]->knob->remove_flag (Push2Knob::ArcToZero);
			gain_meter[s]->meter->hide ();
		}
		n = 6;
		break;
	case Send5:
		for (int s = 0; s < 8; ++s) {
			if (_stripable[s]) {
				gain_meter[s]->knob->set_controllable (_stripable[s]->send_level_controllable (4));
			} else {
				gain_meter[s]->knob->set_controllable (boost::shared_ptr<AutomationControl>());

			}
			gain_meter[s]->knob->remove_flag (Push2Knob::ArcToZero);
			gain_meter[s]->meter->hide ();
		}
		n = 7;
		break;
	default:
		break;
	}

	_upper_backgrounds[n]->set_fill_color (_p2.get_color (Push2::ParameterName));
	_upper_backgrounds[n]->set_outline_color (_p2.get_color (Push2::ParameterName));
	_upper_backgrounds[n]->show ();
	_upper_text[n]->set_color (contrasting_text_color (_p2.get_color (Push2::ParameterName)));
}

void
MixLayout::button_mute ()
{
	boost::shared_ptr<Stripable> s = _session.selection().first_selected_stripable();
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
	boost::shared_ptr<Stripable> s = _session.selection().first_selected_stripable();
	if (s) {
		boost::shared_ptr<AutomationControl> ac = s->solo_control();
		if (ac) {
			_session.set_control (ac, !ac->get_value(), PBD::Controllable::UseGroup);
		}
	}
}

void
MixLayout::button_lower (uint32_t n)
{
	if (!_stripable[n]) {
		return;
	}

	_session.selection().set (_stripable[n], boost::shared_ptr<AutomationControl>());
}

void
MixLayout::strip_vpot (int n, int delta)
{
	boost::shared_ptr<Controllable> ac = gain_meter[n]->knob->controllable();

	if (ac) {
		ac->set_value (
		  ac->interface_to_internal (
		    std::min (ac->upper (),
		              std::max (ac->lower (),
		                        ac->internal_to_interface (ac->get_value ()) +
		                          (delta / 256.0)))),
		  PBD::Controllable::UseGroup);
	}
}

void
MixLayout::strip_vpot_touch (int n, bool touching)
{
	if (_stripable[n]) {
		boost::shared_ptr<AutomationControl> ac = _stripable[n]->gain_control();
		if (ac) {
			const timepos_t now (_session.audible_sample());
			if (touching) {
				ac->start_touch (now);
			} else {
				ac->stop_touch (now);
			}
		}
	}
}

void
MixLayout::stripable_property_change (PropertyChange const& what_changed, uint32_t which)
{
	if (what_changed.contains (Properties::color)) {
		_lower_backgrounds[which]->set_fill_color (_stripable[which]->presentation_info().color());

		if (_stripable[which]->is_selected()) {
			_lower_text[which]->set_fill_color (contrasting_text_color (_stripable[which]->presentation_info().color()));
			/* might not be a MIDI track, in which case this will
			   do nothing
			*/
			_p2.update_selection_color ();
		}
	}

	if (what_changed.contains (Properties::hidden)) {
		switch_bank (_bank_start);
	}

	if (what_changed.contains (Properties::selected)) {

		if (!_stripable[which]) {
			return;
		}

		if (_stripable[which]->is_selected()) {
			show_selection (which);
		} else {
			hide_selection (which);
		}
	}

}

void
MixLayout::show_selection (uint32_t n)
{
	_lower_backgrounds[n]->show ();
	_lower_backgrounds[n]->set_fill_color (_stripable[n]->presentation_info().color());
	_lower_text[n]->set_color (contrasting_text_color (_lower_backgrounds[n]->fill_color()));
}

void
MixLayout::hide_selection (uint32_t n)
{
	_lower_backgrounds[n]->hide ();
	if (_stripable[n]) {
		_lower_text[n]->set_color (_stripable[n]->presentation_info().color());
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
	std::string shortname = short_version (_stripable[n]->name(), 10);
	std::string text;
	boost::shared_ptr<AutomationControl> ac;
	ac = _stripable[n]->solo_control();
	if (ac && ac->get_value()) {
		text += "* ";
	}
	boost::shared_ptr<MuteControl> mc;
	mc = _stripable[n]->mute_control ();
	if (mc) {
		if (mc->muted_by_self_or_masters()) {
			text += "! ";
		} else if (mc->muted_by_others_soloing()) {
			text += "- "; // it would be nice to use Unicode mute"\uD83D\uDD07 ";
		}
	}
	text += shortname;
	_lower_text[n]->set (text);
}

void
MixLayout::switch_bank (uint32_t base)
{
	_stripable_connections.drop_connections ();

	/* work backwards so we can tell if we should actually switch banks */

	boost::shared_ptr<Stripable> s[8];
	uint32_t different = 0;

	for (int n = 0; n < 8; ++n) {
		s[n] = _session.get_remote_nth_stripable (base+n, PresentationInfo::Flag (PresentationInfo::Route|PresentationInfo::VCA));
		if (s[n] != _stripable[n]) {
			different++;
		}
	}

	if (!s[0]) {
		/* not even the first stripable exists, do nothing */
		for (int n = 0; n < 8; ++n) {
			_stripable[n].reset ();
			gain_meter[n]->knob->set_controllable (boost::shared_ptr<AutomationControl>());
			gain_meter[n]->meter->set_meter (0);
		}
		return;
	}

	for (int n = 0; n < 8; ++n) {
		_stripable[n] = s[n];
	}

	/* at least one stripable in this bank */

	_bank_start = base;

	for (int n = 0; n < 8; ++n) {

		if (!_stripable[n]) {
			_lower_text[n]->hide ();
			hide_selection (n);
			gain_meter[n]->knob->set_controllable (boost::shared_ptr<AutomationControl>());
			gain_meter[n]->meter->set_meter (0);
		} else {

			_lower_text[n]->show ();

			/* stripable goes away? refill the bank, starting at the same point */

			_stripable[n]->DropReferences.connect (_stripable_connections, invalidator (*this), boost::bind (&MixLayout::switch_bank, this, _bank_start), &_p2);
			_stripable[n]->presentation_info().PropertyChanged.connect (_stripable_connections, invalidator (*this), boost::bind (&MixLayout::stripable_property_change, this, _1, n), &_p2);
			_stripable[n]->solo_control()->Changed.connect (_stripable_connections, invalidator (*this), boost::bind (&MixLayout::solo_changed, this, n), &_p2);
			_stripable[n]->mute_control()->Changed.connect (_stripable_connections, invalidator (*this), boost::bind (&MixLayout::mute_changed, this, n), &_p2);

			if (_stripable[n]->is_selected()) {
				show_selection (n);
			} else {
				hide_selection (n);
			}

			/* this will set lower text to the correct value (basically
			   the stripable name)
			*/

			solo_mute_changed (n);

			gain_meter[n]->knob->set_text_color (_stripable[n]->presentation_info().color());
			gain_meter[n]->knob->set_arc_start_color (_stripable[n]->presentation_info().color());
			gain_meter[n]->knob->set_arc_end_color (_stripable[n]->presentation_info().color());
		}


		boost::shared_ptr<Push2::Button> b;

		switch (n) {
		case 0:
			b = _p2.button_by_id (Push2::Lower1);
			break;
		case 1:
			b = _p2.button_by_id (Push2::Lower2);
			break;
		case 2:
			b = _p2.button_by_id (Push2::Lower3);
			break;
		case 3:
			b = _p2.button_by_id (Push2::Lower4);
			break;
		case 4:
			b = _p2.button_by_id (Push2::Lower5);
			break;
		case 5:
			b = _p2.button_by_id (Push2::Lower6);
			break;
		case 6:
			b = _p2.button_by_id (Push2::Lower7);
			break;
		case 7:
			b = _p2.button_by_id (Push2::Lower8);
			break;
		}

		if (_stripable[n]) {
			b->set_color (_p2.get_color_index (_stripable[n]->presentation_info().color()));
		} else {
			b->set_color (Push2::LED::Black);
		}

		b->set_state (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}

	show_vpot_mode ();
}

void
MixLayout::button_right ()
{
	switch_bank (std::max (0, _bank_start + 8));
}

void
MixLayout::button_left ()
{
	switch_bank (std::max (0, _bank_start - 8));
}


void
MixLayout::button_select_press ()
{
}

void
MixLayout::button_select_release ()
{
	if (!(_p2.modifier_state() & Push2::ModSelect)) {
		/* somebody else used us as a modifier */
		return;
	}

	int selected = -1;

	for (int n = 0; n < 8; ++n) {
		if (_stripable[n]) {
			if (_stripable[n]->is_selected()) {
					selected = n;
					break;
			}
		}
	}

	if (selected < 0) {

		/* no visible track selected, select first (if any) */

		if (_stripable[0]) {
			_session.selection().set (_stripable[0], boost::shared_ptr<AutomationControl>());
		}

	} else {

		if (_p2.modifier_state() & Push2::ModShift) {
			/* select prev */

			if (selected == 0) {
				/* current selected is leftmost ... cancel selection,
				   switch banks by one, and select leftmost
				*/
				if (_bank_start != 0) {
					_session.selection().clear_stripables ();
					switch_bank (_bank_start - 1);
					if (_stripable[0]) {
						_session.selection().set (_stripable[0], boost::shared_ptr<AutomationControl>());
					}
				}
			} else {
				/* select prev, if any */
				int n = selected - 1;
				while (n >= 0 && !_stripable[n]) {
					--n;
				}
				if (n >= 0) {
					_session.selection().set (_stripable[n], boost::shared_ptr<AutomationControl>());
				}
			}

		} else {

			/* select next */

			if (selected == 7) {
				/* current selected is rightmost ... cancel selection,
				   switch banks by one, and select righmost
				*/
				_session.selection().toggle (_stripable[selected], boost::shared_ptr<AutomationControl>());
				switch_bank (_bank_start + 1);
				if (_stripable[7]) {
					_session.selection().set (_stripable[7], boost::shared_ptr<AutomationControl>());
				}
			} else {
				/* select next, if any */
				int n = selected + 1;
				while (n < 8 && !_stripable[n]) {
					++n;
				}

				if (n != 8) {
					_session.selection().set (_stripable[n], boost::shared_ptr<AutomationControl>());
				}
			}
		}
	}
}

void
MixLayout::stripables_added ()
{
	/* reload current bank */
	switch_bank (_bank_start);
}

void
MixLayout::button_down ()
{
	_p2.scroll_dn_1_track ();
}

void
MixLayout::button_up ()
{
	_p2.scroll_up_1_track ();
}

void
MixLayout::update_meters ()
{
	if (_vpot_mode != Volume) {
		return;
	}

	for (uint32_t n = 0; n < 8; ++n) {
		gain_meter[n]->meter->update_meters ();
	}
}

MixLayout::GainMeter::GainMeter (Item* parent, Push2& p2)
	: Container (parent)
{
	/* knob and meter become owned by their parent on the canvas */

	knob = new Push2Knob (p2, this);
	knob->set_radius (25);
	/* leave position at (0,0) */

	meter = new LevelMeter (p2, this, 90, ArdourCanvas::Meter::Vertical);
	meter->set_position (Duple (40, -60));
}

