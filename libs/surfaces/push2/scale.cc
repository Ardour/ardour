/*
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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
#include "gtkmm2ext/gui_thread.h"

#include "gtkmm2ext/colors.h"
#include "canvas/rectangle.h"
#include "canvas/text.h"

#include "canvas.h"
#include "menu.h"
#include "push2.h"
#include "scale.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace Gtkmm2ext;
using namespace ArdourCanvas;

static double unselected_root_alpha = 0.5;

ScaleLayout::ScaleLayout (Push2& p, Session & s, std::string const & name)
	: Push2Layout (p, s, name)
	, _last_vpot (-1)
	, _vpot_delta_cnt (0)
{
	Pango::FontDescription fd ("Sans 10");

	/* background */

	_bg = new ArdourCanvas::Rectangle (this);
	_bg->set (Rect (0, 0, display_width(), display_height()));
	_bg->set_fill_color (_p2.get_color (Push2::DarkBackground));

	_left_scroll_text = new Text (this);
	_left_scroll_text->set_font_description (fd);
	_left_scroll_text->set_position (Duple (10, 5));
	_left_scroll_text->set_color (_p2.get_color (Push2::LightBackground));

	_close_text = new Text (this);
	_close_text->set_font_description (fd);
	_close_text->set_position (Duple (25, 5));
	_close_text->set_color (_p2.get_color (Push2::LightBackground));
	_close_text->set (_("Close"));

	_right_scroll_text = new Text (this);
	_right_scroll_text->set_font_description (fd);
	_right_scroll_text->set_position (Duple (10 + (7 * Push2Canvas::inter_button_spacing()), 5));
	_right_scroll_text->set_color (_p2.get_color (Push2::LightBackground));

	Pango::FontDescription fd2 ("Sans 8");
	_inkey_text = new Text (this);
	_inkey_text->set_font_description (fd2);
	_inkey_text->set_position (Duple (10, 140));
	_inkey_text->set_color (_p2.get_color (Push2::LightBackground));
	_inkey_text->set (_("InKey"));

	_chromatic_text = new Text (this);
	_chromatic_text->set_font_description (fd2);
	_chromatic_text->set_position (Duple (45, 140));
	_chromatic_text->set_color (_p2.get_color (Push2::LightBackground));
	_chromatic_text->set (_("Chromatic"));

	for (int n = 0; n < 8; ++n) {

		/* text labels for root notes etc.*/

		Text* t = new Text (this);
		t->set_font_description (fd);
		t->set_color (change_alpha (_p2.get_color (Push2::LightBackground), unselected_root_alpha));
		t->set_position (Duple (10 + (n * Push2Canvas::inter_button_spacing()), 5));

		switch (n) {
		case 0:
			/* zeroth element is a dummy */
			break;
		case 1:
			t->set (S_("Note|C"));
			break;
		case 2:
			t->set (S_("Note|G"));
			break;
		case 3:
			t->set (S_("Note|D"));
			break;
		case 4:
			t->set (S_("Note|A"));
			break;
		case 5:
			t->set (S_("Note|E"));
			break;
		case 6:
			t->set (S_("Note|B"));
			break;
		}

		_upper_text.push_back (t);

		t = new Text (this);
		t->set_font_description (fd);
		t->set_color (change_alpha (_p2.get_color (Push2::LightBackground), unselected_root_alpha));
		t->set_position (Duple (10 + (n*Push2Canvas::inter_button_spacing()), 140));

		switch (n) {
		case 0:
			/* zeroth element is a dummy */
			break;
		case 1:
			t->set (S_("Note|F"));
			break;
		case 2:
			t->set (S_("Note|B\u266D/A\u266F"));
			break;
		case 3:
			t->set (S_("Note|E\u266D/D\u266F"));
			break;
		case 4:
			t->set (S_("Note|A\u266D/G\u266F"));
			break;
		case 5:
			t->set (S_("Note|D\u266D/C\u266F"));
			break;
		case 6:
			t->set (S_("Note|G\u266D/F\u266F"));
			break;
		}

		_lower_text.push_back (t);
	}

	build_scale_menu ();

	_p2.ScaleChange.connect (_p2_connections, invalidator (*this), boost::bind (&ScaleLayout::show_root_state, this), &_p2);
}

ScaleLayout::~ScaleLayout ()
{
}

void
ScaleLayout::render (Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	render_children (area, context);
}

void
ScaleLayout::button_upper (uint32_t n)
{
	if (n == 0) {
		if (_scale_menu->can_scroll_left()) {
			_scale_menu->scroll (Push2Menu::DirectionLeft, true);
		} else {
			_p2.use_previous_layout ();
		}
		return;
	}

	if (n == 7) {
		_scale_menu->scroll (Push2Menu::DirectionRight, true);
		return;
	}

	int root = 0;

	switch (n) {
	case 1:
		/* C */
		root = 0;
		break;
	case 2:
		/* G */
		root = 7;
		break;
	case 3:
		/* D */
		root = 2;
		break;
	case 4:
		/* A */
		root = 9;
		break;
	case 5:
		/* E */
		root = 4;
		break;
	case 6:
		/* B */
		root = 11;
		break;
	case 7:
		/* unused */
		return;
	}

	_p2.set_pad_scale (root, _p2.root_octave(), _p2.mode(), _p2.in_key());
}

void
ScaleLayout::button_lower (uint32_t n)
{
	if (n == 0) {
		_p2.set_pad_scale (_p2.scale_root(), _p2.root_octave(), _p2.mode(), !_p2.in_key());
		return;
	}

	int root = 0;

	switch (n) {
	case 1:
		/* F */
		root = 5;
		break;
	case 2:
		/* B-flat */
		root = 10;
		break;
	case 3:
		/* E flat */
		root = 3;
		break;
	case 4:
		/* A flat */
		root = 8;
		break;
	case 5:
		/* D flat */
		root = 1;
		break;
	case 6:
		/* G flat */
		root = 6;
		break;
	case 7:
		/* fixed mode */
		return;
	}

	_p2.set_pad_scale (root, _p2.root_octave(), _p2.mode(), _p2.in_key());
}

void
ScaleLayout::button_up ()
{
	_scale_menu->scroll (Push2Menu::DirectionUp);
}

void
ScaleLayout::button_down ()
{
	_scale_menu->scroll (Push2Menu::DirectionDown);
}

void
ScaleLayout::button_left ()
{
	_scale_menu->scroll (Push2Menu::DirectionLeft);
}

void
ScaleLayout::button_right ()
{
	_scale_menu->scroll (Push2Menu::DirectionRight);
}

void
ScaleLayout::show ()
{
	boost::shared_ptr<Push2::Button> b;

	_last_vpot = -1;

	b = _p2.button_by_id (Push2::Upper1);
	b->set_color (Push2::LED::White);
	b->set_state (Push2::LED::OneShot24th);
	_p2.write (b->state_msg());

	b = _p2.button_by_id (Push2::Upper8);
	b->set_color (Push2::LED::White);
	b->set_state (Push2::LED::OneShot24th);
	_p2.write (b->state_msg());

	b = _p2.button_by_id (Push2::Lower1);
	b->set_color (Push2::LED::White);
	b->set_state (Push2::LED::OneShot24th);
	_p2.write (b->state_msg());

	/* all root buttons should be dimly lit */

	Push2::ButtonID root_buttons[] = { Push2::Upper2, Push2::Upper3, Push2::Upper4, Push2::Upper5, Push2::Upper6, Push2::Upper7,
	                                   Push2::Lower2, Push2::Lower3, Push2::Lower4, Push2::Lower5, Push2::Lower6, Push2::Lower7, };

	for (size_t n = 0; n < sizeof (root_buttons) / sizeof (root_buttons[0]); ++n) {
		b = _p2.button_by_id (root_buttons[n]);

		b->set_color (Push2::LED::DarkGray);
		b->set_state (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}

	show_root_state ();

	Container::show ();
}

void
ScaleLayout::strip_vpot (int n, int delta)
{
	/* menu starts under the 2nd-from-left vpot */

	if (n == 0) {
		return;
	}

	if (_last_vpot != n) {
		uint32_t effective_column = n - 1;
		uint32_t active = _scale_menu->active ();

		if (active / _scale_menu->rows() != effective_column) {
			/* knob turned is different than the current active column.
			   Just change that.
			*/
			_scale_menu->set_active (effective_column * _scale_menu->rows()); /* top entry of that column */
			return;
		}

		/* new vpot, reset delta cnt */

		_vpot_delta_cnt = 0;
	}

	if ((delta < 0 && _vpot_delta_cnt > 0) || (delta > 0 && _vpot_delta_cnt < 0)) {
		/* direction changed, reset */
		_vpot_delta_cnt = 0;
	}

	_vpot_delta_cnt += delta;
	_last_vpot = n;

	/* this thins out vpot delta events so that we don't scroll so fast
	   through the menu.
	*/

	const int vpot_slowdown_factor = 4;

	if ((_vpot_delta_cnt < 0) && (_vpot_delta_cnt % vpot_slowdown_factor == 0)) {
		_scale_menu->scroll (Push2Menu::DirectionUp);
	} else if (_vpot_delta_cnt % vpot_slowdown_factor == 0) {
		_scale_menu->scroll (Push2Menu::DirectionDown);
	}
}

void
ScaleLayout::build_scale_menu ()
{
	vector<string> v;

	/* must match in which enums are declared in push2.h
	 */

	v.push_back ("Dorian");
	v.push_back ("Ionian (Major)");
	v.push_back ("Aeolian (Minor)");
	v.push_back ("Harmonic Minor");
	v.push_back ("MelodicMinor Asc.");
	v.push_back ("MelodicMinor Desc.");
	v.push_back ("Phrygian");
	v.push_back ("Lydian");
	v.push_back ("Mixolydian");
	v.push_back ("Locrian");
	v.push_back ("Pentatonic Major");
	v.push_back ("Pentatonic Minor");
	v.push_back ("Chromatic");
	v.push_back ("Blues Scale");
	v.push_back ("Neapolitan Minor");
	v.push_back ("Neapolitan Major");
	v.push_back ("Oriental");
	v.push_back ("Double Harmonic");
	v.push_back ("Enigmatic");
	v.push_back ("Hirajoshi");
	v.push_back ("Hungarian Minor");
	v.push_back ("Hungarian Major");
	v.push_back ("Kumoi");
	v.push_back ("Iwato");
	v.push_back ("Hindu");
	v.push_back ("Spanish 8 Tone");
	v.push_back ("Pelog");
	v.push_back ("Hungarian Gypsy");
	v.push_back ("Overtone");
	v.push_back ("Leading Whole Tone");
	v.push_back ("Arabian");
	v.push_back ("Balinese");
	v.push_back ("Gypsy");
	v.push_back ("Mohammedan");
	v.push_back ("Javanese");
	v.push_back ("Persian");
	v.push_back ("Algeria");

	_scale_menu = new Push2Menu (this, v);
	_scale_menu->Rearranged.connect (_menu_connections, invalidator (*this), boost::bind (&ScaleLayout::menu_rearranged, this), &_p2);

	_scale_menu->set_layout (6, 6);
	_scale_menu->set_text_color (_p2.get_color (Push2::ParameterName));
	_scale_menu->set_active_color (_p2.get_color (Push2::LightBackground));

	Pango::FontDescription fd ("Sans Bold 8");
	_scale_menu->set_font_description (fd);

	/* move menu into position so that its leftmost column is in the
	 * 2nd-from-left column of the display/button layout.
	 */

	_scale_menu->set_position (Duple (10 + Push2Canvas::inter_button_spacing(), 40));

	/* listen for changes */

	_scale_menu->ActiveChanged.connect (_menu_connections, invalidator (*this), boost::bind (&ScaleLayout::mode_changed, this), &_p2);
}

void
ScaleLayout::show_root_state ()
{
	if (!parent()) {
		/* don't do this stuff if we're not visible */
		return;
	}

	if (_p2.in_key()) {
		_chromatic_text->set_color (change_alpha (_chromatic_text->color(), unselected_root_alpha));
		_inkey_text->set_color (change_alpha (_inkey_text->color(), 1.0));
	} else {
		_inkey_text->set_color (change_alpha (_chromatic_text->color(), unselected_root_alpha));
		_chromatic_text->set_color (change_alpha (_inkey_text->color(), 1.0));
	}

	Pango::FontDescription fd_bold ("Sans Bold 10");
	Pango::FontDescription fd ("Sans 10");

	uint32_t highlight_text = 0;
	vector<Text*>* none_text_array = 0;
	vector<Text*>* one_text_array = 0;
	Push2::ButtonID bid = Push2::Upper2; /* keep compilers quiet */

	switch (_p2.scale_root()) {
	case 0:
		highlight_text = 1;
		none_text_array = &_lower_text;
		one_text_array = &_upper_text;
		bid = Push2::Upper2;
		break;
	case 1:
		highlight_text = 5;
		none_text_array = &_lower_text;
		one_text_array = &_upper_text;
		bid = Push2::Lower6;
		break;
	case 2:
		highlight_text = 3;
		none_text_array = &_lower_text;
		one_text_array = &_upper_text;
		bid = Push2::Upper4;
		break;
	case 3:
		highlight_text = 3;
		none_text_array = &_upper_text;
		one_text_array = &_lower_text;
		bid = Push2::Lower4;
		break;
	case 4:
		highlight_text = 5;
		none_text_array = &_lower_text;
		one_text_array = &_upper_text;
		bid = Push2::Upper6;
		break;
	case 5:
		highlight_text = 1;
		none_text_array = &_upper_text;
		one_text_array = &_lower_text;
		bid = Push2::Lower2;
		break;
	case 6:
		highlight_text = 6;
		none_text_array = &_upper_text;
		one_text_array = &_lower_text;
		bid = Push2::Lower7;
		break;
	case 7:
		highlight_text = 2;
		none_text_array = &_lower_text;
		one_text_array = &_upper_text;
		bid = Push2::Upper3;
		break;
	case 8:
		highlight_text = 4;
		none_text_array = &_upper_text;
		one_text_array = &_lower_text;
		bid = Push2::Lower5;
		break;
	case 9:
		highlight_text = 4;
		none_text_array = &_lower_text;
		one_text_array = &_upper_text;
		bid = Push2::Upper5;
		break;
	case 10:
		highlight_text = 2;
		none_text_array = &_upper_text;
		one_text_array = &_lower_text;
		bid = Push2::Lower3;
		break;
	case 11:
		highlight_text = 6;
		none_text_array = &_lower_text;
		one_text_array = &_upper_text;
		bid = Push2::Upper7;
		break;
	default:
		return;
	}

	if (none_text_array) {

		for (uint32_t nn = 1; nn < 7; ++nn) {
			(*none_text_array)[nn]->set_font_description (fd);
			(*none_text_array)[nn]->set_color (change_alpha ((*none_text_array)[nn]->color(), unselected_root_alpha));

			if (nn == highlight_text) {
				(*one_text_array)[nn]->set_font_description (fd_bold);
				(*one_text_array)[nn]->set_color (change_alpha ((*one_text_array)[nn]->color(), 1.0));
			} else {
				(*one_text_array)[nn]->set_font_description (fd);
				(*one_text_array)[nn]->set_color (change_alpha ((*one_text_array)[nn]->color(), unselected_root_alpha));
			}
		}

	}

	boost::shared_ptr<Push2::Button> b = _p2.button_by_id (bid);

	if (b != _root_button) {
		if (_root_button) {
			/* turn the old one off (but not totally) */
			_root_button->set_color (Push2::LED::DarkGray);
			_root_button->set_state (Push2::LED::OneShot24th);
			_p2.write (_root_button->state_msg());
		}

		_root_button = b;

		if (_root_button) {
			/* turn the new one on */
			_root_button->set_color (Push2::LED::White);
			_root_button->set_state (Push2::LED::OneShot24th);
			_p2.write (_root_button->state_msg());
		}
	}

	_scale_menu->set_active ((uint32_t) _p2.mode ());
}

void
ScaleLayout::mode_changed ()
{
	MusicalMode::Type m = (MusicalMode::Type) _scale_menu->active();
	_p2.set_pad_scale (_p2.scale_root(), _p2.root_octave(), m, _p2.in_key());
}

void
ScaleLayout::menu_rearranged ()
{
	if (_scale_menu->can_scroll_left()) {
		_left_scroll_text->set ("<");
		_close_text->hide ();
	} else {
		_left_scroll_text->set (string());
		_close_text->show ();
	}

	if (_scale_menu->can_scroll_right()) {
		_right_scroll_text->set (">");
	} else {
		_right_scroll_text->set (string());
	}
}

void
ScaleLayout::update_cursor_buttons ()
{
	boost::shared_ptr<Push2::Button> b;
	bool change;

	b = _p2.button_by_id (Push2::Up);
	change = false;

	if (_scale_menu->active() == 0) {
		if (b->color_index() != Push2::LED::Black) {
			b->set_color (Push2::LED::Black);
			change = true;
		}
	} else {
		if (b->color_index() != Push2::LED::White) {
			b->set_color (Push2::LED::White);
			change = true;
		}
	}

	if (change) {
		b->set_state (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}

	/* down */

	b = _p2.button_by_id (Push2::Down);
	change = false;

	if (_scale_menu->active() == _scale_menu->items() - 1) {
		if (b->color_index() != Push2::LED::Black) {
			b->set_color (Push2::LED::Black);
			change = true;
		}
	} else {
		if (b->color_index() != Push2::LED::White) {
			b->set_color (Push2::LED::White);
			change = true;
		}

	}
	if (change) {
		b->set_color (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}

	/* left */

	b = _p2.button_by_id (Push2::Left);
	change = false;

	if (_scale_menu->active() < _scale_menu->rows()) {
		if (b->color_index() != Push2::LED::Black) {
			b->set_color (Push2::LED::Black);
			change = true;
		}
	} else {
		if (b->color_index() != Push2::LED::White) {
			b->set_color (Push2::LED::White);
			change = true;
		}

	}
	if (change) {
		b->set_color (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}

	/* right */

	b = _p2.button_by_id (Push2::Right);
	change = false;

	if (_scale_menu->active() > (_scale_menu->items() - _scale_menu->rows())) {
		if (b->color_index() != Push2::LED::Black) {
			b->set_color (Push2::LED::Black);
			change = true;
		}
	} else {
		if (b->color_index() != Push2::LED::White) {
			b->set_color (Push2::LED::White);
			change = true;
		}

	}

	if (change) {
		b->set_color (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}
}
