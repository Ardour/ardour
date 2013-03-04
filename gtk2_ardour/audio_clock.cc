/*
    Copyright (C) 1999 Paul Davis

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

#include <cstdio> // for sprintf
#include <cmath>

#include "pbd/convert.h"
#include "pbd/enumwriter.h"

#include <gtkmm/style.h>
#include <sigc++/bind.h>

#include "gtkmm2ext/cairocell.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/slave.h"
#include "ardour/tempo.h"
#include "ardour/types.h"

#include "ardour_ui.h"
#include "audio_clock.h"
#include "global_signals.h"
#include "utils.h"
#include "keyboard.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;

using Gtkmm2ext::Keyboard;

sigc::signal<void> AudioClock::ModeChanged;
vector<AudioClock*> AudioClock::clocks;
const double AudioClock::info_font_scale_factor = 0.50;
const double AudioClock::separator_height = 0.0;
const double AudioClock::x_leading_padding = 6.0;

#define BBT_BAR_CHAR "|"
#define BBT_SCANF_FORMAT "%" PRIu32 "%*c%" PRIu32 "%*c%" PRIu32
#define INFO_FONT_SIZE ((int)lrint(font_size * info_font_scale_factor))
#define TXTSPAN "<span font-family=\"Sans\" foreground=\"white\">"

AudioClock::AudioClock (const string& clock_name, bool transient, const string& widget_name,
			bool allow_edit, bool follows_playhead, bool duration, bool with_info)
	: ops_menu (0)
	, _name (clock_name)
	, is_transient (transient)
	, is_duration (duration)
	, editable (allow_edit)
	, _follows_playhead (follows_playhead)
	, _off (false)
	, em_width (0)
	, _edit_by_click_field (false)
	, _negative_allowed (false)
	, edit_is_negative (false)
	, editing_attr (0)
	, foreground_attr (0)
	, first_height (0)
	, first_width (0)
	, style_resets_first (true)
	, layout_height (0)
	, layout_width (0)
	, info_height (0)
	, upper_height (0)
	, mode_based_info_ratio (1.0)
	, corner_radius (4)
	, font_size (10240)
	, editing (false)
	, bbt_reference_time (-1)
	, last_when(0)
	, last_pdelta (0)
	, last_sdelta (0)
	, dragging (false)
	, drag_field (Field (0))
{
	set_flags (CAN_FOCUS);

	_layout = Pango::Layout::create (get_pango_context());
	_layout->set_attributes (normal_attributes);

	if (with_info) {
		_left_layout = Pango::Layout::create (get_pango_context());
		_right_layout = Pango::Layout::create (get_pango_context());
	}

	set_widget_name (widget_name);

	_mode = BBT; /* lie to force mode switch */
	set_mode (Timecode);
	set (last_when, true);

	if (!is_transient) {
		clocks.push_back (this);
	}

	ColorsChanged.connect (sigc::mem_fun (*this, &AudioClock::set_colors));
	DPIReset.connect (sigc::mem_fun (*this, &AudioClock::dpi_reset));
}

AudioClock::~AudioClock ()
{
	delete foreground_attr;
	delete editing_attr;
}

void
AudioClock::set_widget_name (const string& str)
{
	if (str.empty()) {
		set_name ("clock");
	} else {
		set_name (str + " clock");
	}

	if (is_realized()) {
		set_colors ();
	}
}


void
AudioClock::on_realize ()
{
	Gtk::Requisition req;

	CairoWidget::on_realize ();
	
	set_clock_dimensions (req);

	first_width = req.width;
	first_height = req.height;

	set_font ();
	set_colors ();
}

void
AudioClock::set_font ()
{
	Glib::RefPtr<Gtk::Style> style = get_style ();
	Pango::FontDescription font;
	Pango::AttrFontDesc* font_attr;

	if (!is_realized()) {
		font = get_font_for_style (get_name());
	} else {
		font = style->get_font();
	}

	font_size = font.get_size();

	font_attr = new Pango::AttrFontDesc (Pango::Attribute::create_attr_font_desc (font));

	normal_attributes.change (*font_attr);
	editing_attributes.change (*font_attr);

	/* now a smaller version of the same font */

	delete font_attr;
	font.set_size (INFO_FONT_SIZE);
	font.set_weight (Pango::WEIGHT_NORMAL);
	font_attr = new Pango::AttrFontDesc (Pango::Attribute::create_attr_font_desc (font));

	info_attributes.change (*font_attr);

	/* and an even smaller one */

	delete font_attr;

	/* get the figure width for the font. This doesn't have to super
	 * accurate since we only use it to measure the (roughly 1 character)
	 * offset from the position Pango tells us for the "cursor"
	 */

	Glib::RefPtr<Pango::Layout> tmp = Pango::Layout::create (get_pango_context());
	int ignore_height;

	tmp->set_text ("8");
	tmp->get_pixel_size (em_width, ignore_height);

	/* force redraw of markup with new font-size */
	set (last_when, true);
}

void
AudioClock::set_active_state (Gtkmm2ext::ActiveState s)
{
	CairoWidget::set_active_state (s);
	set_colors ();
}

void
AudioClock::set_colors ()
{
	int r, g, b, a;

	uint32_t bg_color;
	uint32_t text_color;
	uint32_t editing_color;
	uint32_t cursor_color;

	if (active_state()) {
		bg_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 active: background", get_name()));
		text_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 active: text", get_name()));
		editing_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 active: edited text", get_name()));
		cursor_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1 active: cursor", get_name()));
	} else {
		bg_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: background", get_name()));
		text_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: text", get_name()));
		editing_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: edited text", get_name()));
		cursor_color = ARDOUR_UI::config()->color_by_name (string_compose ("%1: cursor", get_name()));
	}

	/* store for bg and cursor in render() */

	UINT_TO_RGBA (bg_color, &r, &g, &b, &a);

	bg_r = r/255.0;
	bg_g = g/255.0;
	bg_b = b/255.0;
	bg_a = a/255.0;

	UINT_TO_RGBA (cursor_color, &r, &g, &b, &a);

	cursor_r = r/255.0;
	cursor_g = g/255.0;
	cursor_b = b/255.0;
	cursor_a = a/255.0;

	/* rescale for Pango colors ... sigh */

	r = lrint (r * 65535.0);
	g = lrint (g * 65535.0);
	b = lrint (b * 65535.0);

	UINT_TO_RGBA (text_color, &r, &g, &b, &a);
	r = lrint ((r/255.0) * 65535.0);
	g = lrint ((g/255.0) * 65535.0);
	b = lrint ((b/255.0) * 65535.0);
	foreground_attr = new Pango::AttrColor (Pango::Attribute::create_attr_foreground (r, g, b));

	UINT_TO_RGBA (editing_color, &r, &g, &b, &a);
	r = lrint ((r/255.0) * 65535.0);
	g = lrint ((g/255.0) * 65535.0);
	b = lrint ((b/255.0) * 65535.0);
	editing_attr = new Pango::AttrColor (Pango::Attribute::create_attr_foreground (r, g, b));

	normal_attributes.change (*foreground_attr);
	info_attributes.change (*foreground_attr);
	editing_attributes.change (*foreground_attr);
	editing_attributes.change (*editing_attr);

	if (!editing) {
		_layout->set_attributes (normal_attributes);
	} else {
		_layout->set_attributes (editing_attributes);
	}

	queue_draw ();
}

void
AudioClock::render (cairo_t* cr)
{
	/* main layout: rounded rect, plus the text */

	if (_need_bg) {
		cairo_set_source_rgba (cr, bg_r, bg_g, bg_b, bg_a);
		if (corner_radius) {
			if (_left_layout) {
				Gtkmm2ext::rounded_top_half_rectangle (cr, 0, 0, get_width(), upper_height, corner_radius);
			} else {
				Gtkmm2ext::rounded_rectangle (cr, 0, 0, get_width(), upper_height, corner_radius);
			}
		} else {
			cairo_rectangle (cr, 0, 0, get_width(), upper_height);
		}
		cairo_fill (cr);
	}

	cairo_move_to (cr, (get_width() - layout_width) / 2.0, (upper_height - layout_height) / 2.0);

	pango_cairo_show_layout (cr, _layout->gobj());

	if (_left_layout) {

		double h = get_height() - upper_height - separator_height;

		if (_need_bg) {
			cairo_set_source_rgba (cr, bg_r, bg_g, bg_b, bg_a);
		}

		if (mode_based_info_ratio != 1.0) {

			double left_rect_width = round (((get_width() - separator_height) * mode_based_info_ratio) + 0.5);

			if (_need_bg) {
				if (corner_radius) {
					Gtkmm2ext::rounded_bottom_half_rectangle (cr, 0, upper_height + separator_height,
							left_rect_width + (separator_height == 0 ? corner_radius : 0),
							h, corner_radius);
				} else {
					cairo_rectangle (cr, 0, upper_height + separator_height, left_rect_width, h);
				}
				cairo_fill (cr);
			}

			cairo_move_to (cr, x_leading_padding, upper_height + separator_height + ((h - info_height)/2.0));
			pango_cairo_show_layout (cr, _left_layout->gobj());

			if (_need_bg) {
				if (corner_radius) {
					Gtkmm2ext::rounded_bottom_half_rectangle (cr, left_rect_width + separator_height,
							upper_height + separator_height,
							get_width() - separator_height - left_rect_width,
							h, corner_radius);
				} else {
					cairo_rectangle (cr, left_rect_width + separator_height, upper_height + separator_height,
							 get_width() - separator_height - left_rect_width, h);
				}
				cairo_fill (cr);
			}


			if (_right_layout->get_alignment() == Pango::ALIGN_RIGHT) {
				/* right-align does not work per se beacuse layout width is unset.
				 * Using _right_layout->set_width([value >=0]) would also enable
				 * word-wrapping which is not wanted here.
				 * The solution is to custom align the layout depending on its size.
				 * if it is larger than the available space it will be cropped on the
				 * right edge rather than override text on the left side.
				 */
				int x, rw, rh;
				_right_layout->get_pixel_size(rw, rh);
				x = get_width() - rw - separator_height - x_leading_padding;
				if (x < x_leading_padding + left_rect_width + separator_height) {
					/* rather cut off the right end than overlap with the text on the left */
					x = x_leading_padding + left_rect_width + separator_height;
				}
				cairo_move_to (cr, x, upper_height + separator_height + ((h - info_height)/2.0));
			} else {
				cairo_move_to (cr, x_leading_padding + left_rect_width + separator_height, upper_height + separator_height + ((h - info_height)/2.0));
			}
			pango_cairo_show_layout (cr, _right_layout->gobj());

		} else {
			/* no info to display, or just one */

			if (_need_bg) {
				if (corner_radius) {
					Gtkmm2ext::rounded_bottom_half_rectangle (cr, 0, upper_height + separator_height, get_width(), h, corner_radius);
				} else {
					cairo_rectangle (cr, 0, upper_height + separator_height, get_width(), h);
				}
				cairo_fill (cr);
			}
		}
	}

	if (editing) {
		if (!insert_map.empty()) {

			int xcenter = (get_width() - layout_width) /2;

			if (input_string.length() < insert_map.size()) {
				Pango::Rectangle cursor;

				if (input_string.empty()) {
					/* nothing entered yet, put cursor at the end
					   of string
					*/
					cursor = _layout->get_cursor_strong_pos (edit_string.length() - 1);
				} else {
					cursor = _layout->get_cursor_strong_pos (insert_map[input_string.length()]);
				}

				cairo_set_source_rgba (cr, cursor_r, cursor_g, cursor_b, cursor_a);
				cairo_rectangle (cr,
						 min (get_width() - 2.0,
						      (double) xcenter + cursor.get_x()/PANGO_SCALE + em_width),
						 (upper_height - layout_height)/2.0,
						 2.0, cursor.get_height()/PANGO_SCALE);
				cairo_fill (cr);
			} else {
				/* we've entered all possible digits, no cursor */
			}

		} else {
			if (input_string.empty()) {
				cairo_set_source_rgba (cr, cursor_r, cursor_g, cursor_b, cursor_a);
				cairo_rectangle (cr,
						 (get_width()/2.0),
						 (upper_height - layout_height)/2.0,
						 2.0, upper_height);
				cairo_fill (cr);
			}
		}
	}
}

void
AudioClock::on_size_allocate (Gtk::Allocation& alloc)
{
	CairoWidget::on_size_allocate (alloc);

	if (_left_layout) {
		upper_height = (get_height()/2.0) - 1.0;
	} else {
		upper_height = get_height();
	}
}

void
AudioClock::set_clock_dimensions (Gtk::Requisition& req)
{
	Glib::RefPtr<Pango::Layout> tmp;
	Glib::RefPtr<Gtk::Style> style = get_style ();
	Pango::FontDescription font;

	tmp = Pango::Layout::create (get_pango_context());

	if (!is_realized()) {
		font = get_font_for_style (get_name());
	} else {
		font = style->get_font();
	}

	tmp->set_font_description (font);

	/* this string is the longest thing we will ever display */
	tmp->set_text (" 88:88:88,888");
	tmp->get_pixel_size (req.width, req.height);

	layout_height = req.height;
	layout_width = req.width;
}

void
AudioClock::on_size_request (Gtk::Requisition* req)
{
	/* even for non fixed width clocks, the size we *ask* for never changes,
	   even though the size we receive might. so once we've computed it,
	   just return it.
	*/

	if (first_width) {
		req->width = first_width;
		req->height = first_height;
		return;
	}

	set_clock_dimensions (*req);

	/* now tackle height, for which we need to know the height of the lower
	 * layout
	 */

	if (_left_layout) {

		Glib::RefPtr<Pango::Layout> tmp;
		Glib::RefPtr<Gtk::Style> style = get_style ();
		Pango::FontDescription font;
		int w;
		
		tmp = Pango::Layout::create (get_pango_context());
		
		if (!is_realized()) {
			font = get_font_for_style (get_name());
		} else {
			font = style->get_font();
		}
		
		tmp->set_font_description (font);

		font.set_size (INFO_FONT_SIZE);
		font.set_weight (Pango::WEIGHT_NORMAL);
		tmp->set_font_description (font);

		/* we only care about height, so put as much stuff in here
		   as possible that might change the height.
		*/
		tmp->set_text ("qyhH|"); /* one ascender, one descender */

		tmp->get_pixel_size (w, info_height);

		/* silly extra padding that seems necessary to correct the info
		 * that pango just gave us. I have no idea why.
		 */

		req->height += info_height;
		req->height += separator_height;
	}
}

void
AudioClock::show_edit_status (int length)
{
	editing_attr->set_start_index (edit_string.length() - length);
	editing_attr->set_end_index (edit_string.length());

	editing_attributes.change (*foreground_attr);
	editing_attributes.change (*editing_attr);

	_layout->set_attributes (editing_attributes);
}

void
AudioClock::start_edit (Field f)
{
	if (!editing) {
		pre_edit_string = _layout->get_text ();
		if (!insert_map.empty()) {
			edit_string = pre_edit_string;
		} else {
			edit_string.clear ();
			_layout->set_text ("");
		}
		
		input_string.clear ();
		editing = true;
		edit_is_negative = false;
		
		if (f) {
			input_string = get_field (f);
			show_edit_status (merge_input_and_edit_string ());
			_layout->set_text (edit_string);
		}
		
		queue_draw ();

		Keyboard::magic_widget_grab_focus ();
		grab_focus ();
	}
}

string
AudioClock::get_field (Field f)
{
	switch (f) {
	case Timecode_Hours:
		return edit_string.substr (1, 2);
		break;
	case Timecode_Minutes:
		return edit_string.substr (4, 2);
		break;
	case Timecode_Seconds:
		return edit_string.substr (7, 2);
		break;
	case Timecode_Frames:
		return edit_string.substr (10, 2);
		break;
	case MS_Hours:
		return edit_string.substr (1, 2);
		break;
	case MS_Minutes:
		return edit_string.substr (4, 2);
		break;
	case MS_Seconds:
		return edit_string.substr (7, 2);
		break;
	case MS_Milliseconds:
		return edit_string.substr (10, 3);
		break;
	case Bars:
		return edit_string.substr (1, 3);
		break;
	case Beats:
		return edit_string.substr (5, 2);
		break;
	case Ticks:
		return edit_string.substr (8, 4);
		break;
	case AudioFrames:
		return edit_string;
		break;
	}
	return "";
}

void
AudioClock::end_edit (bool modify)
{
	if (modify) {

		bool ok = true;

		switch (_mode) {
		case Timecode:
			ok = timecode_validate_edit (edit_string);
			break;

		case BBT:
			ok = bbt_validate_edit (edit_string);
			break;

		case MinSec:
			ok = minsec_validate_edit (edit_string);
			break;

		case Frames:
			break;
		}

		if (!ok) {
			edit_string = pre_edit_string;
			input_string.clear ();
			_layout->set_text (edit_string);
			show_edit_status (0);
			/* edit attributes remain in use */
		} else {

			editing = false;
			framepos_t pos = 0; /* stupid gcc */

			switch (_mode) {
			case Timecode:
				pos = frames_from_timecode_string (edit_string);
				break;

			case BBT:
				if (is_duration) {
					pos = frame_duration_from_bbt_string (0, edit_string);
				} else {
					pos = frames_from_bbt_string (0, edit_string);
				}
				break;

			case MinSec:
				pos = frames_from_minsec_string (edit_string);
				break;

			case Frames:
				pos = frames_from_audioframes_string (edit_string);
				break;
			}

			set (pos, true);
			_layout->set_attributes (normal_attributes);
			ValueChanged(); /* EMIT_SIGNAL */
		}

	} else {

		editing = false;
		edit_is_negative = false;
		_layout->set_attributes (normal_attributes);
		_layout->set_text (pre_edit_string);
	}

	queue_draw ();

	if (!editing) {
		drop_focus ();
	}
}

void
AudioClock::drop_focus ()
{
	Keyboard::magic_widget_drop_focus ();

	if (has_focus()) {

		/* move focus back to the default widget in the top level window */

		Widget* top = get_toplevel();

		if (top->is_toplevel ()) {
			Window* win = dynamic_cast<Window*> (top);
			win->grab_focus ();
		}
	}
}

framecnt_t
AudioClock::parse_as_frames_distance (const std::string& str)
{
	framecnt_t f;

	if (sscanf (str.c_str(), "%" PRId64, &f) == 1) {
		return f;
	}

	return 0;
}

framecnt_t
AudioClock::parse_as_minsec_distance (const std::string& str)
{
	framecnt_t sr = _session->frame_rate();
	int msecs;
	int secs;
	int mins;
	int hrs;

	switch (str.length()) {
	case 0:
		return 0;
	case 1:
	case 2:
	case 3:
	case 4:
		sscanf (str.c_str(), "%" PRId32, &msecs);
		return msecs * (sr / 1000);

	case 5:
		sscanf (str.c_str(), "%1" PRId32 "%" PRId32, &secs, &msecs);
		return (secs * sr) + (msecs * (sr/1000));

	case 6:
		sscanf (str.c_str(), "%2" PRId32 "%" PRId32, &secs, &msecs);
		return (secs * sr) + (msecs * (sr/1000));

	case 7:
		sscanf (str.c_str(), "%1" PRId32 "%2" PRId32 "%" PRId32, &mins, &secs, &msecs);
		return (mins * 60 * sr) + (secs * sr) + (msecs * (sr/1000));

	case 8:
		sscanf (str.c_str(), "%2" PRId32 "%2" PRId32 "%" PRId32, &mins, &secs, &msecs);
		return (mins * 60 * sr) + (secs * sr) + (msecs * (sr/1000));

	case 9:
		sscanf (str.c_str(), "%1" PRId32 "%2" PRId32 "%2" PRId32 "%" PRId32, &hrs, &mins, &secs, &msecs);
		return (hrs * 3600 * sr) + (mins * 60 * sr) + (secs * sr) + (msecs * (sr/1000));

	case 10:
		sscanf (str.c_str(), "%1" PRId32 "%2" PRId32 "%2" PRId32 "%" PRId32, &hrs, &mins, &secs, &msecs);
		return (hrs * 3600 * sr) + (mins * 60 * sr) + (secs * sr) + (msecs * (sr/1000));

	default:
		break;
	}

	return 0;
}

framecnt_t
AudioClock::parse_as_timecode_distance (const std::string& str)
{
	double fps = _session->timecode_frames_per_second();
	framecnt_t sr = _session->frame_rate();
	int frames;
	int secs;
	int mins;
	int hrs;

	switch (str.length()) {
	case 0:
		return 0;
	case 1:
	case 2:
		sscanf (str.c_str(), "%" PRId32, &frames);
		return lrint ((frames/(float)fps) * sr);

	case 3:
		sscanf (str.c_str(), "%1" PRId32 "%" PRId32, &secs, &frames);
		return (secs * sr) + lrint ((frames/(float)fps) * sr);

	case 4:
		sscanf (str.c_str(), "%2" PRId32 "%" PRId32, &secs, &frames);
		return (secs * sr) + lrint ((frames/(float)fps) * sr);

	case 5:
		sscanf (str.c_str(), "%1" PRId32 "%2" PRId32 "%" PRId32, &mins, &secs, &frames);
		return (mins * 60 * sr) + (secs * sr) + lrint ((frames/(float)fps) * sr);

	case 6:
		sscanf (str.c_str(), "%2" PRId32 "%2" PRId32 "%" PRId32, &mins, &secs, &frames);
		return (mins * 60 * sr) + (secs * sr) + lrint ((frames/(float)fps) * sr);

	case 7:
		sscanf (str.c_str(), "%1" PRId32 "%2" PRId32 "%2" PRId32 "%" PRId32, &hrs, &mins, &secs, &frames);
		return (hrs * 3600 * sr) + (mins * 60 * sr) + (secs * sr) + lrint ((frames/(float)fps) * sr);

	case 8:
		sscanf (str.c_str(), "%2" PRId32 "%2" PRId32 "%2" PRId32 "%" PRId32, &hrs, &mins, &secs, &frames);
		return (hrs * 3600 * sr) + (mins * 60 * sr) + (secs * sr) + lrint ((frames/(float)fps) * sr);

	default:
		break;
	}

	return 0;
}

framecnt_t
AudioClock::parse_as_bbt_distance (const std::string&)
{
	return 0;
}

framecnt_t
AudioClock::parse_as_distance (const std::string& instr)
{
	switch (_mode) {
	case Timecode:
		return parse_as_timecode_distance (instr);
		break;
	case Frames:
		return parse_as_frames_distance (instr);
		break;
	case BBT:
		return parse_as_bbt_distance (instr);
		break;
	case MinSec:
		return parse_as_minsec_distance (instr);
		break;
	}
	return 0;
}

void
AudioClock::end_edit_relative (bool add)
{
	bool ok = true;

	switch (_mode) {
	case Timecode:
		ok = timecode_validate_edit (edit_string);
		break;

	case BBT:
		ok = bbt_validate_edit (edit_string);
		break;

	case MinSec:
		ok = minsec_validate_edit (edit_string);
		break;

	case Frames:
		break;
	}

	if (!ok) {
		edit_string = pre_edit_string;
		input_string.clear ();
		_layout->set_text (edit_string);
		show_edit_status (0);
		/* edit attributes remain in use */
		queue_draw ();
		return;
	}

	framecnt_t frames = parse_as_distance (input_string);

	editing = false;

	editing = false;
	_layout->set_attributes (normal_attributes);

	if (frames != 0) {
		if (add) {
			set (current_time() + frames, true);
		} else {
			framepos_t c = current_time();

			if (c > frames || _negative_allowed) {
				set (c - frames, true);
			} else {
				set (0, true);
			}
		}
		ValueChanged (); /* EMIT SIGNAL */
	}

	input_string.clear ();
	queue_draw ();
	drop_focus ();
}

void
AudioClock::session_property_changed (const PropertyChange&)
{
	set (last_when, true);
}

void
AudioClock::session_configuration_changed (std::string p)
{
	if (_negative_allowed) {
		/* session option editor clock */
		return;
	}

	if (p == "sync-source" || p == "external-sync") {
		set (current_time(), true);
		return;
	}

	if (p != "timecode-offset" && p != "timecode-offset-negative") {
		return;
	}

	framecnt_t current;

	switch (_mode) {
	case Timecode:
		if (is_duration) {
			current = current_duration ();
		} else {
			current = current_time ();
		}
		set (current, true);
		break;
	default:
		break;
	}
}

void
AudioClock::set (framepos_t when, bool force, framecnt_t offset)
{
 	if ((!force && !is_visible()) || _session == 0) {
		return;
	}

	if (is_duration) {
		when = when - offset;
	}

	if (when == last_when && !force) {
		if (_mode != Timecode && _mode != MinSec) {
			/* may need to force display of TC source
			 * time, so don't return early.
			 */
			return;
		}
	}

	if (!editing) {
		if (_right_layout) {
			_right_layout->set_alignment(Pango::ALIGN_LEFT);
		}

		switch (_mode) {
		case Timecode:
			if (_right_layout) {
				_right_layout->set_alignment(Pango::ALIGN_RIGHT);
			}
			set_timecode (when, force);
			break;

		case BBT:
			set_bbt (when, force);
			break;

		case MinSec:
			if (_right_layout) {
				_right_layout->set_alignment(Pango::ALIGN_RIGHT);
			}
			set_minsec (when, force);
			break;

		case Frames:
			set_frames (when, force);
			break;
		}
	}

	queue_draw ();
	last_when = when;
}

void
AudioClock::set_slave_info ()
{
	if (!_left_layout || !_right_layout) {
		return;
	}

	SyncSource sync_src = Config->get_sync_source();

	if (_session->config.get_external_sync()) {
		Slave* slave = _session->slave();

		switch (sync_src) {
		case JACK:
			_left_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2</span></span>",
						INFO_FONT_SIZE, sync_source_to_string(sync_src, true)));
			_right_layout->set_text ("");
			break;
		case MIDIClock:
			if (slave) {
				_left_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2</span></span>",
							INFO_FONT_SIZE, sync_source_to_string(sync_src, true)));
				_right_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2</span></span>",
							INFO_FONT_SIZE, slave->approximate_current_delta()));
			} else {
				_left_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2</span></span>",
							INFO_FONT_SIZE, _("--pending--")));
				_right_layout->set_text ("");
			}
			break;
		case LTC:
		case MTC:
			if (slave) {
				bool matching;
				TimecodeSlave* tcslave;
				if ((tcslave = dynamic_cast<TimecodeSlave*>(_session->slave())) != 0) {
					matching = (tcslave->apparent_timecode_format() == _session->config.get_timecode_format());
					_left_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2</span><span foreground=\"%3\">%4</span></span>",
										  INFO_FONT_SIZE, sync_source_to_string(sync_src, true)[0], (matching?"green":"red"),
										  dynamic_cast<TimecodeSlave*>(slave)->approximate_current_position()));
					_right_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2</span></span>",
										   INFO_FONT_SIZE, slave->approximate_current_delta()));
				}
			} else {
				_left_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2</span></span>",
							INFO_FONT_SIZE, _("--pending--")));
				_right_layout->set_text ("");
			}
			break;
		}
	} else {
		_left_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"INT/%2</span></span>",
					INFO_FONT_SIZE, sync_source_to_string(sync_src, true)));
		_right_layout->set_text ("");
	}
}

void
AudioClock::set_frames (framepos_t when, bool /*force*/)
{
	char buf[32];
	bool negative = false;

	if (_off) {
		_layout->set_text (" ----------");

		if (_left_layout) {
			_left_layout->set_text ("");
			_right_layout->set_text ("");
		}

		return;
	}

	if (when < 0) {
		when = -when;
		negative = true;
	}

	if (negative) {
		snprintf (buf, sizeof (buf), "-%10" PRId64, when);
	} else {
		snprintf (buf, sizeof (buf), " %10" PRId64, when);
	}

	_layout->set_text (buf);

	if (_left_layout) {
		framecnt_t rate = _session->frame_rate();

		if (fmod (rate, 100.0) == 0.0) {
			sprintf (buf, "%.1fkHz", rate/1000.0);
		} else {
			sprintf (buf, "%" PRId64 "Hz", rate);
		}

		_left_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2 </span><span foreground=\"green\">%3</span></span>",
				INFO_FONT_SIZE, _("SR"), buf));

		float vid_pullup = _session->config.get_video_pullup();

		if (vid_pullup == 0.0) {
			_right_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2 </span><span foreground=\"green\">off</span></span>",
					INFO_FONT_SIZE, _("Pull")));
		} else {
			sprintf (buf, _("%+.4f%%"), vid_pullup);
			_right_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%2 </span><span foreground=\"green\">%3</span></span>",
					INFO_FONT_SIZE, _("Pull"), buf));
		}
	}
}

void
AudioClock::set_minsec (framepos_t when, bool /*force*/)
{
	char buf[32];
	framecnt_t left;
	int hrs;
	int mins;
	int secs;
	int millisecs;
	bool negative = false;

	if (_off) {
		_layout->set_text (" --:--:--.---");

		if (_left_layout) {
			_left_layout->set_text ("");
			_right_layout->set_text ("");
		}

		return;
	}

	if (when < 0) {
		when = -when;
		negative = true;
	}

	left = when;
	hrs = (int) floor (left / (_session->frame_rate() * 60.0f * 60.0f));
	left -= (framecnt_t) floor (hrs * _session->frame_rate() * 60.0f * 60.0f);
	mins = (int) floor (left / (_session->frame_rate() * 60.0f));
	left -= (framecnt_t) floor (mins * _session->frame_rate() * 60.0f);
	secs = (int) floor (left / (float) _session->frame_rate());
	left -= (framecnt_t) floor (secs * _session->frame_rate());
	millisecs = floor (left * 1000.0 / (float) _session->frame_rate());

	if (negative) {
		snprintf (buf, sizeof (buf), "-%02" PRId32 ":%02" PRId32 ":%02" PRId32 ".%03" PRId32, hrs, mins, secs, millisecs);
	} else {
		snprintf (buf, sizeof (buf), " %02" PRId32 ":%02" PRId32 ":%02" PRId32 ".%03" PRId32, hrs, mins, secs, millisecs);
	}

	_layout->set_text (buf);
	set_slave_info();
}

void
AudioClock::set_timecode (framepos_t when, bool /*force*/)
{
	Timecode::Time TC;
	bool negative = false;

	if (_off) {
		_layout->set_text (" --:--:--:--");
		if (_left_layout) {
			_left_layout->set_text ("");
			_right_layout->set_text ("");
		}

		return;
	}

	if (when < 0) {
		when = -when;
		negative = true;
	}

	if (is_duration) {
		_session->timecode_duration (when, TC);
	} else {
		_session->timecode_time (when, TC);
	}

	TC.negative = TC.negative || negative;

	_layout->set_text (Timecode::timecode_format_time(TC));

	set_slave_info();
}

void
AudioClock::set_bbt (framepos_t when, bool /*force*/)
{
	char buf[16];
	Timecode::BBT_Time BBT;
	bool negative = false;

	if (_off) {
		_layout->set_text (" ---|--|----");
		if (_left_layout) {
			_left_layout->set_text ("");
			_right_layout->set_text ("");
		}
		return;
	}

	if (when < 0) {
		when = -when;
		negative = true;
	}

	/* handle a common case */
	if (is_duration) {
		if (when == 0) {
			BBT.bars = 0;
			BBT.beats = 0;
			BBT.ticks = 0;
		} else {
			_session->tempo_map().bbt_time (when, BBT);
			BBT.bars--;
			BBT.beats--;
		}
	} else {
		_session->tempo_map().bbt_time (when, BBT);
	}

	if (negative) {
		snprintf (buf, sizeof (buf), "-%03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
			  BBT.bars, BBT.beats, BBT.ticks);
	} else {
		snprintf (buf, sizeof (buf), " %03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
			  BBT.bars, BBT.beats, BBT.ticks);
	}

	_layout->set_text (buf);

	if (_right_layout) {
		framepos_t pos;

		if (bbt_reference_time < 0) {
			pos = when;
		} else {
			pos = bbt_reference_time;
		}

		TempoMetric m (_session->tempo_map().metric_at (pos));

		sprintf (buf, "%-5.2f", m.tempo().beats_per_minute());
		_left_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%3</span> <span foreground=\"green\">%2</span></span>",
							  INFO_FONT_SIZE, buf, _("Tempo")));

		sprintf (buf, "%g/%g", m.meter().divisions_per_bar(), m.meter().note_divisor());
		_right_layout->set_markup (string_compose ("<span size=\"%1\">"TXTSPAN"%3</span> <span foreground=\"green\">%2</span></span>",
							   INFO_FONT_SIZE, buf, _("Meter")));
	}
}

void
AudioClock::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {

		_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&AudioClock::session_configuration_changed, this, _1), gui_context());
		_session->tempo_map().PropertyChanged.connect (_session_connections, invalidator (*this), boost::bind (&AudioClock::session_property_changed, this, _1), gui_context());

		const XMLProperty* prop;
		XMLNode* node = _session->extra_xml (X_("ClockModes"));
		AudioClock::Mode amode;

		if (node) {
			for (XMLNodeList::const_iterator i = node->children().begin(); i != node->children().end(); ++i) {
				if ((prop = (*i)->property (X_("name"))) && prop->value() == _name) {

					if ((prop = (*i)->property (X_("mode"))) != 0) {
						amode = AudioClock::Mode (string_2_enum (prop->value(), amode));
						set_mode (amode);
					}
					if ((prop = (*i)->property (X_("on"))) != 0) {
						set_off (!string_is_affirmative (prop->value()));
					}
					break;
				}
			}
		}

		set (last_when, true);
	}
}

bool
AudioClock::on_key_press_event (GdkEventKey* ev)
{
	if (!editing) {
		return false;
	}

	string new_text;
	char new_char = 0;
	int highlight_length;
	framepos_t pos;

	switch (ev->keyval) {
	case GDK_0:
	case GDK_KP_0:
		new_char = '0';
		break;
	case GDK_1:
	case GDK_KP_1:
		new_char = '1';
		break;
	case GDK_2:
	case GDK_KP_2:
		new_char = '2';
		break;
	case GDK_3:
	case GDK_KP_3:
		new_char = '3';
		break;
	case GDK_4:
	case GDK_KP_4:
		new_char = '4';
		break;
	case GDK_5:
	case GDK_KP_5:
		new_char = '5';
		break;
	case GDK_6:
	case GDK_KP_6:
		new_char = '6';
		break;
	case GDK_7:
	case GDK_KP_7:
		new_char = '7';
		break;
	case GDK_8:
	case GDK_KP_8:
		new_char = '8';
		break;
	case GDK_9:
	case GDK_KP_9:
		new_char = '9';
		break;

	case GDK_minus:
	case GDK_KP_Subtract:
		if (_negative_allowed && input_string.empty()) {
				edit_is_negative = true;
				edit_string.replace(0,1,"-");
				_layout->set_text (edit_string);
				queue_draw ();
		} else {
			end_edit_relative (false);
		}
		return true;
		break;

	case GDK_plus:
		end_edit_relative (true);
		return true;
		break;

	case GDK_Tab:
	case GDK_Return:
	case GDK_KP_Enter:
		end_edit (true);
		return true;
		break;

	case GDK_Escape:
		end_edit (false);
		ChangeAborted();  /*  EMIT SIGNAL  */
		return true;

	case GDK_Delete:
	case GDK_BackSpace:
		if (!input_string.empty()) {
			/* delete the last key entered
			*/
			input_string = input_string.substr (0, input_string.length() - 1);
		}
		goto use_input_string;

	default:
		return false;
	}

	if (!insert_map.empty() && (input_string.length() >= insert_map.size())) {
		/* too many digits: eat the key event, but do nothing with it */
		return true;
	}

	input_string.push_back (new_char);

  use_input_string:

	switch (_mode) {
	case Frames:
		/* get this one in the right order, and to the right width */
		if (ev->keyval == GDK_Delete || ev->keyval == GDK_BackSpace) {
			edit_string = edit_string.substr (0, edit_string.length() - 1);
		} else {
			edit_string.push_back (new_char);
		}
		if (!edit_string.empty()) {
			char buf[32];
			sscanf (edit_string.c_str(), "%" PRId64, &pos);
			snprintf (buf, sizeof (buf), " %10" PRId64, pos);
			edit_string = buf;
		}
		/* highlight the whole thing */
		highlight_length = edit_string.length();
		break;

	default:
		highlight_length = merge_input_and_edit_string ();
	}

	if (edit_is_negative) {
		edit_string.replace(0,1,"-");
	} else {
		if (!pre_edit_string.empty() && (pre_edit_string.at(0) == '-')) {
			edit_string.replace(0,1,"_");
		} else {
			edit_string.replace(0,1," ");
		}
	}

	show_edit_status (highlight_length);
	_layout->set_text (edit_string);
	queue_draw ();

	return true;
}

int
AudioClock::merge_input_and_edit_string ()
{
	/* merge with pre-edit-string into edit string */

	edit_string = pre_edit_string;

	if (input_string.empty()) {
		return 0;
	}

	string::size_type target;
	for (string::size_type i = 0; i < input_string.length(); ++i) {
		target = insert_map[input_string.length() - 1 - i];
		edit_string[target] = input_string[i];
	}
	/* highlight from end to wherever the last character was added */
	return edit_string.length() - insert_map[input_string.length()-1];
}


bool
AudioClock::on_key_release_event (GdkEventKey *ev)
{
	if (!editing) {
		return false;
	}

	/* return true for keys that we used on press
	   so that they cannot possibly do double-duty
	*/
	switch (ev->keyval) {
	case GDK_0:
	case GDK_KP_0:
	case GDK_1:
	case GDK_KP_1:
	case GDK_2:
	case GDK_KP_2:
	case GDK_3:
	case GDK_KP_3:
	case GDK_4:
	case GDK_KP_4:
	case GDK_5:
	case GDK_KP_5:
	case GDK_6:
	case GDK_KP_6:
	case GDK_7:
	case GDK_KP_7:
	case GDK_8:
	case GDK_KP_8:
	case GDK_9:
	case GDK_KP_9:
	case GDK_period:
	case GDK_comma:
	case GDK_KP_Decimal:
	case GDK_Tab:
	case GDK_Return:
	case GDK_KP_Enter:
	case GDK_Escape:
	case GDK_minus:
	case GDK_plus:
	case GDK_KP_Add:
	case GDK_KP_Subtract:
		return true;
	default:
		return false;
	}
}

AudioClock::Field
AudioClock::index_to_field (int index) const
{
	switch (_mode) {
	case Timecode:
		if (index < 4) {
			return Timecode_Hours;
		} else if (index < 7) {
			return Timecode_Minutes;
		} else if (index < 10) {
			return Timecode_Seconds;
		} else {
			return Timecode_Frames;
		}
		break;
	case BBT:
		if (index < 5) {
			return Bars;
		} else if (index < 7) {
			return Beats;
		} else {
			return Ticks;
		}
		break;
	case MinSec:
		if (index < 3) {
			return Timecode_Hours;
		} else if (index < 6) {
			return MS_Minutes;
		} else if (index < 9) {
			return MS_Seconds;
		} else {
			return MS_Milliseconds;
		}
		break;
	case Frames:
		return AudioFrames;
		break;
	}

	return Field (0);
}

bool
AudioClock::on_button_press_event (GdkEventButton *ev)
{
	switch (ev->button) {
	case 1:
		if (editable && !_off) {
			int index;
			int trailing;
			int y;
			int x;

			/* the text has been centered vertically, so adjust
			 * x and y.
			 */
			int xcenter = (get_width() - layout_width) /2;

			y = ev->y - ((upper_height - layout_height)/2);
			x = ev->x - xcenter;

			if (!_layout->xy_to_index (x * PANGO_SCALE, y * PANGO_SCALE, index, trailing)) {
				/* pretend it is a character on the far right */
				index = 99;
			}
			drag_field = index_to_field (index);
			dragging = true;
			/* make absolutely sure that the pointer is grabbed */
			gdk_pointer_grab(ev->window,false ,
					 GdkEventMask( Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK |Gdk::BUTTON_RELEASE_MASK),
					 NULL,NULL,ev->time);
			drag_accum = 0;
			drag_start_y = ev->y;
			drag_y = ev->y;
		}
		break;

	default:
		return false;
		break;
	}

	return true;
}

bool
AudioClock::on_button_release_event (GdkEventButton *ev)
{
	if (editable && !_off) {
		if (dragging) {
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			dragging = false;
			if (ev->y > drag_start_y+1 || ev->y < drag_start_y-1 || Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)){
				// we actually dragged so return without
				// setting editing focus, or we shift clicked
				return true;
			} else {
				if (ev->button == 1) {

					if (_edit_by_click_field) {

						int xcenter = (get_width() - layout_width) /2;
						int index = 0;
						int trailing;
						int y = ev->y - ((upper_height - layout_height)/2);
						int x = ev->x - xcenter;
						Field f;

						if (!_layout->xy_to_index (x * PANGO_SCALE, y * PANGO_SCALE, index, trailing)) {
							return true;
						}

						f = index_to_field (index);

						switch (f) {
						case Timecode_Frames:
						case MS_Milliseconds:
						case Ticks:
							f = Field (0);
							break;
						default:
							break;
						}
						start_edit (f);
					} else {
						start_edit ();
					}
				}
			}
		}
	}

	if (Keyboard::is_context_menu_event (ev)) {
		if (ops_menu == 0) {
			build_ops_menu ();
		}
		ops_menu->popup (1, ev->time);
		return true;
	}

	return false;
}

bool
AudioClock::on_focus_out_event (GdkEventFocus* ev)
{
	bool ret = CairoWidget::on_focus_out_event (ev);

	if (editing) {
		end_edit (false);
	}

	return ret;
}

bool
AudioClock::on_scroll_event (GdkEventScroll *ev)
{
	int index;
	int trailing;

	if (editing || _session == 0 || !editable || _off) {
		return false;
	}

	int y;
	int x;

	/* the text has been centered vertically, so adjust
	 * x and y.
	 */

	int xcenter = (get_width() - layout_width) /2;
	y = ev->y - ((upper_height - layout_height)/2);
	x = ev->x - xcenter;

	if (!_layout->xy_to_index (x * PANGO_SCALE, y * PANGO_SCALE, index, trailing)) {
		/* not in the main layout */
		return false;
	}

	Field f = index_to_field (index);
	framepos_t frames = 0;

	switch (ev->direction) {

	case GDK_SCROLL_UP:
		frames = get_frame_step (f);
		if (frames != 0) {
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
				frames *= 10;
			}
			set (current_time() + frames, true);
			ValueChanged (); /* EMIT_SIGNAL */
		}
		break;

	case GDK_SCROLL_DOWN:
		frames = get_frame_step (f);
		if (frames != 0) {
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
				frames *= 10;
			}

			if (!_negative_allowed && (double)current_time() - (double)frames < 0.0) {
				set (0, true);
			} else {
				set (current_time() - frames, true);
			}

			ValueChanged (); /* EMIT_SIGNAL */
		}
		break;

	default:
		return false;
		break;
	}

	return true;
}

bool
AudioClock::on_motion_notify_event (GdkEventMotion *ev)
{
	if (editing || _session == 0 || !dragging) {
		return false;
	}

	float pixel_frame_scale_factor = 0.2f;

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier))  {
		pixel_frame_scale_factor = 0.1f;
	}


	if (Keyboard::modifier_state_contains (ev->state,
					       Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)) {

		pixel_frame_scale_factor = 0.025f;
	}

	double y_delta = ev->y - drag_y;

	drag_accum +=  y_delta*pixel_frame_scale_factor;

	drag_y = ev->y;

	if (trunc (drag_accum) != 0) {

		framepos_t frames;
		framepos_t pos;
		int dir;
		dir = (drag_accum < 0 ? 1:-1);
		pos = current_time();
		frames = get_frame_step (drag_field, pos, dir);

		if (frames  != 0 &&  frames * drag_accum < current_time()) {
			set ((framepos_t) floor (pos - drag_accum * frames), false); // minus because up is negative in GTK
		} else {
			set (0 , false);
 		}

	       	drag_accum= 0;
		ValueChanged();	 /* EMIT_SIGNAL */
	}

	return true;
}

framepos_t
AudioClock::get_frame_step (Field field, framepos_t pos, int dir)
{
	framecnt_t f = 0;
	Timecode::BBT_Time BBT;
	switch (field) {
	case Timecode_Hours:
		f = (framecnt_t) floor (3600.0 * _session->frame_rate());
		break;
	case Timecode_Minutes:
		f = (framecnt_t) floor (60.0 * _session->frame_rate());
		break;
	case Timecode_Seconds:
		f = _session->frame_rate();
		break;
	case Timecode_Frames:
		f = (framecnt_t) floor (_session->frame_rate() / _session->timecode_frames_per_second());
		break;

	case AudioFrames:
		f = 1;
		break;

	case MS_Hours:
		f = (framecnt_t) floor (3600.0 * _session->frame_rate());
		break;
	case MS_Minutes:
		f = (framecnt_t) floor (60.0 * _session->frame_rate());
		break;
	case MS_Seconds:
		f = (framecnt_t) _session->frame_rate();
		break;
	case MS_Milliseconds:
		f = (framecnt_t) floor (_session->frame_rate() / 1000.0);
		break;

	case Bars:
		BBT.bars = 1;
		BBT.beats = 0;
		BBT.ticks = 0;
		f = _session->tempo_map().bbt_duration_at (pos,BBT,dir);
		break;
	case Beats:
		BBT.bars = 0;
		BBT.beats = 1;
		BBT.ticks = 0;
		f = _session->tempo_map().bbt_duration_at(pos,BBT,dir);
		break;
	case Ticks:
		BBT.bars = 0;
		BBT.beats = 0;
		BBT.ticks = 1;
		f = _session->tempo_map().bbt_duration_at(pos,BBT,dir);
		break;
	default:
		error << string_compose (_("programming error: %1"), "attempt to get frames from non-text field!") << endmsg;
		f = 0;
		break;
	}

	return f;
}

framepos_t
AudioClock::current_time (framepos_t) const
{
	return last_when;
}

framepos_t
AudioClock::current_duration (framepos_t pos) const
{
	framepos_t ret = 0;

	switch (_mode) {
	case Timecode:
		ret = last_when;
		break;
	case BBT:
		ret = frame_duration_from_bbt_string (pos, _layout->get_text());
		break;

	case MinSec:
		ret = last_when;
		break;

	case Frames:
		ret = last_when;
		break;
	}

	return ret;
}

bool
AudioClock::bbt_validate_edit (const string& str)
{
	AnyTime any;

	if (sscanf (str.c_str(), BBT_SCANF_FORMAT, &any.bbt.bars, &any.bbt.beats, &any.bbt.ticks) != 3) {
		return false;
	}

	if (any.bbt.ticks > Timecode::BBT_Time::ticks_per_beat) {
		return false;
	}

	if (!is_duration && any.bbt.bars == 0) {
		return false;
	}

	if (!is_duration && any.bbt.beats == 0) {
		return false;
	}

	return true;
}

bool
AudioClock::timecode_validate_edit (const string&)
{
	Timecode::Time TC;
	int hours;
	char ignored[2];

	if (sscanf (_layout->get_text().c_str(), "%[- _]%" PRId32 ":%" PRId32 ":%" PRId32 "%[:;]%" PRId32,
		    ignored, &hours, &TC.minutes, &TC.seconds, ignored, &TC.frames) != 6) {
		return false;
	}

	if (hours < 0) {
		TC.hours = hours * -1;
		TC.negative = true;
	} else {
		TC.hours = hours;
		TC.negative = false;
	}

	if (TC.negative && !_negative_allowed) {
		return false;
	}

	if (TC.hours > 23U || TC.minutes > 59U || TC.seconds > 59U) {
		return false;
	}

	if (TC.frames > (uint32_t) rint (_session->timecode_frames_per_second()) - 1) {
		return false;
	}

	if (_session->timecode_drop_frames()) {
		if (TC.minutes % 10 && TC.seconds == 0U && TC.frames < 2U) {
			return false;
		}
	}

	return true;
}

bool
AudioClock::minsec_validate_edit (const string& str)
{
	int hrs, mins, secs, millisecs;

	if (sscanf (str.c_str(), "%d:%d:%d.%d", &hrs, &mins, &secs, &millisecs) != 4) {
		return false;
	}

	if (hrs > 23 || mins > 59 || secs > 59 || millisecs > 999) {
		return false;
	}

	return true;
}

framepos_t
AudioClock::frames_from_timecode_string (const string& str) const
{
	if (_session == 0) {
		return 0;
	}

	Timecode::Time TC;
	framepos_t sample;
	char ignored[2];
	int hours;

	if (sscanf (str.c_str(), "%[- _]%d:%d:%d%[:;]%d", ignored, &hours, &TC.minutes, &TC.seconds, ignored, &TC.frames) != 6) {
		error << string_compose (_("programming error: %1 %2"), "badly formatted timecode clock string", str) << endmsg;
		return 0;
	}
	TC.hours = abs(hours);
	TC.rate = _session->timecode_frames_per_second();
	TC.drop= _session->timecode_drop_frames();

	_session->timecode_to_sample (TC, sample, false /* use_offset */, false /* use_subframes */ );

	// timecode_tester ();
	if (edit_is_negative) {
		sample = - sample;
	}

	return sample;
}

framepos_t
AudioClock::frames_from_minsec_string (const string& str) const
{
	if (_session == 0) {
		return 0;
	}

	int hrs, mins, secs, millisecs;
	framecnt_t sr = _session->frame_rate();

	if (sscanf (str.c_str(), "%d:%d:%d.%d", &hrs, &mins, &secs, &millisecs) != 4) {
		error << string_compose (_("programming error: %1 %2"), "badly formatted minsec clock string", str) << endmsg;
		return 0;
	}

	return (framepos_t) floor ((hrs * 60.0f * 60.0f * sr) + (mins * 60.0f * sr) + (secs * sr) + (millisecs * sr / 1000.0));
}

framepos_t
AudioClock::frames_from_bbt_string (framepos_t pos, const string& str) const
{
	if (_session == 0) {
		error << "AudioClock::current_time() called with BBT mode but without session!" << endmsg;
		return 0;
	}

	AnyTime any;
	any.type = AnyTime::BBT;

	if (sscanf (str.c_str(), BBT_SCANF_FORMAT, &any.bbt.bars, &any.bbt.beats, &any.bbt.ticks) != 3) {
		return 0;
	}

	if (is_duration) {
		any.bbt.bars++;
		any.bbt.beats++;
                return _session->any_duration_to_frames (pos, any);
	} else {
                return _session->convert_to_frames (any);
        }
}


framepos_t
AudioClock::frame_duration_from_bbt_string (framepos_t pos, const string& str) const
{
	if (_session == 0) {
		error << "AudioClock::current_time() called with BBT mode but without session!" << endmsg;
		return 0;
	}

	Timecode::BBT_Time bbt;

	if (sscanf (str.c_str(), BBT_SCANF_FORMAT, &bbt.bars, &bbt.beats, &bbt.ticks) != 3) {
		return 0;
	}

	return _session->tempo_map().bbt_duration_at(pos,bbt,1);
}

framepos_t
AudioClock::frames_from_audioframes_string (const string& str) const
{
	framepos_t f;
	sscanf (str.c_str(), "%" PRId64, &f);
	return f;
}

void
AudioClock::build_ops_menu ()
{
	using namespace Menu_Helpers;
	ops_menu = new Menu;
	MenuList& ops_items = ops_menu->items();
	ops_menu->set_name ("ArdourContextMenu");

	if (!Profile->get_sae()) {
		ops_items.push_back (MenuElem (_("Timecode"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), Timecode)));
	}
	ops_items.push_back (MenuElem (_("Bars:Beats"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), BBT)));
	ops_items.push_back (MenuElem (_("Minutes:Seconds"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), MinSec)));
	ops_items.push_back (MenuElem (_("Samples"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), Frames)));

	if (editable && !_off && !is_duration && !_follows_playhead) {
		ops_items.push_back (SeparatorElem());
		ops_items.push_back (MenuElem (_("Set From Playhead"), sigc::mem_fun(*this, &AudioClock::set_from_playhead)));
		ops_items.push_back (MenuElem (_("Locate to This Time"), sigc::mem_fun(*this, &AudioClock::locate)));
	}
}

void
AudioClock::set_from_playhead ()
{
	if (!_session) {
		return;
	}

	set (_session->transport_frame());
	ValueChanged ();
}

void
AudioClock::locate ()
{
	if (!_session || is_duration) {
		return;
	}

	_session->request_locate (current_time(), _session->transport_rolling ());
}

void
AudioClock::set_mode (Mode m)
{
	if (_mode == m) {
		return;
	}

	_mode = m;

	insert_map.clear();

	_layout->set_text ("");

	if (_left_layout) {

		_left_layout->set_attributes (info_attributes);
		_right_layout->set_attributes (info_attributes);
		/* adjust info_height according to font size */
		int ignored;
		_left_layout->set_text (" 1234567890");
		_left_layout->get_pixel_size (ignored, info_height);

		_left_layout->set_text ("");
		_right_layout->set_text ("");
	}

	switch (_mode) {
	case Timecode:
		mode_based_info_ratio = 0.6;
		insert_map.push_back (11);
		insert_map.push_back (10);
		insert_map.push_back (8);
		insert_map.push_back (7);
		insert_map.push_back (5);
		insert_map.push_back (4);
		insert_map.push_back (2);
		insert_map.push_back (1);
		break;

	case BBT:
		mode_based_info_ratio = 0.5;
		insert_map.push_back (11);
		insert_map.push_back (10);
		insert_map.push_back (9);
		insert_map.push_back (8);
		insert_map.push_back (6);
		insert_map.push_back (5);
		insert_map.push_back (3);
		insert_map.push_back (2);
		insert_map.push_back (1);
		break;

	case MinSec:
		mode_based_info_ratio = 0.6;
		insert_map.push_back (12);
		insert_map.push_back (11);
		insert_map.push_back (10);
		insert_map.push_back (8);
		insert_map.push_back (7);
		insert_map.push_back (5);
		insert_map.push_back (4);
		insert_map.push_back (2);
		insert_map.push_back (1);
		break;

	case Frames:
		mode_based_info_ratio = 0.45;
		break;
	}

	set (last_when, true);

        if (!is_transient) {
                ModeChanged (); /* EMIT SIGNAL (the static one)*/
        }

        mode_changed (); /* EMIT SIGNAL (the member one) */
}

void
AudioClock::set_bbt_reference (framepos_t pos)
{
	bbt_reference_time = pos;
}

void
AudioClock::on_style_changed (const Glib::RefPtr<Gtk::Style>& old_style)
{
	CairoWidget::on_style_changed (old_style);

	Gtk::Requisition req;
	set_clock_dimensions (req);

	set_font ();
	set_colors ();
}

void
AudioClock::set_editable (bool yn)
{
	editable = yn;
}

void
AudioClock::set_is_duration (bool yn)
{
	if (yn == is_duration) {
		return;
	}

	is_duration = yn;
	set (last_when, true);
}

void
AudioClock::set_off (bool yn)
{
	if (_off == yn) {
		return;
	}

	_off = yn;

	/* force a redraw. last_when will be preserved, but the clock text will
	 * change
	 */

	set (last_when, true);
}

void
AudioClock::focus ()
{
	start_edit (Field (0));
}

void
AudioClock::set_corner_radius (double r)
{
	corner_radius = r;
	first_width = 0;
	first_height = 0;
	queue_resize ();
}

void
AudioClock::dpi_reset ()
{
	/* force recomputation of size even if we are fixed width
	 */
	first_width = 0;
	first_height = 0;
	queue_resize ();
}

void
AudioClock::set_negative_allowed (bool yn)
{
	_negative_allowed = yn;
}
