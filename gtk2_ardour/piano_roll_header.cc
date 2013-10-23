/*
    Copyright (C) 2008 Paul Davis

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

#include <iostream>
#include "evoral/midi_events.h"
#include "ardour/midi_track.h"

#include "gtkmm2ext/keyboard.h"

#include "editing.h"
#include "piano_roll_header.h"
#include "midi_time_axis.h"
#include "midi_streamview.h"
#include "public_editor.h"

const int no_note = 0xff;

using namespace std;
using namespace Gtkmm2ext;

PianoRollHeader::Color PianoRollHeader::white = PianoRollHeader::Color(0.77f, 0.78f, 0.76f);
PianoRollHeader::Color PianoRollHeader::white_highlight = PianoRollHeader::Color(0.87f, 0.88f, 0.86f);
PianoRollHeader::Color PianoRollHeader::white_shade_light = PianoRollHeader::Color(0.95f, 0.95f, 0.95f);
PianoRollHeader::Color PianoRollHeader::white_shade_dark = PianoRollHeader::Color(0.56f, 0.56f, 0.56f);

PianoRollHeader::Color PianoRollHeader::black = PianoRollHeader::Color(0.24f, 0.24f, 0.24f);
PianoRollHeader::Color PianoRollHeader::black_highlight = PianoRollHeader::Color(0.30f, 0.30f, 0.30f);
PianoRollHeader::Color PianoRollHeader::black_shade_light = PianoRollHeader::Color(0.46f, 0.46f, 0.46f);
PianoRollHeader::Color PianoRollHeader::black_shade_dark = PianoRollHeader::Color(0.1f, 0.1f, 0.1f);

PianoRollHeader::Color::Color()
	: r(1.0f)
	, g(1.0f)
	, b(1.0f)
{
}

PianoRollHeader::Color::Color(double _r, double _g, double _b)
	: r(_r)
	, g(_g)
	, b(_b)
{
}

inline void
PianoRollHeader::Color::set(const PianoRollHeader::Color& c)
{
	r = c.r;
	g = c.g;
	b = c.b;
}

PianoRollHeader::PianoRollHeader(MidiStreamView& v)
	: _view(v)
	, _highlighted_note(no_note)
	, _clicked_note(no_note)
	, _dragging(false)
{
	add_events (Gdk::BUTTON_PRESS_MASK |
		    Gdk::BUTTON_RELEASE_MASK |
		    Gdk::POINTER_MOTION_MASK |
		    Gdk::ENTER_NOTIFY_MASK |
		    Gdk::LEAVE_NOTIFY_MASK |
		    Gdk::SCROLL_MASK);

	for (int i = 0; i < 128; ++i) {
		_active_notes[i] = false;
	}

	_view.NoteRangeChanged.connect (sigc::mem_fun (*this, &PianoRollHeader::note_range_changed));
}

inline void
create_path(Cairo::RefPtr<Cairo::Context> cr, double x[], double y[], int start, int stop)
{
	cr->move_to(x[start], y[start]);

	for (int i = start+1; i <= stop; ++i) {
		cr->line_to(x[i], y[i]);
	}
}

inline void
render_rect(Cairo::RefPtr<Cairo::Context> cr, int /*note*/, double x[], double y[],
	     PianoRollHeader::Color& bg, PianoRollHeader::Color& tl_shadow, PianoRollHeader::Color& br_shadow)
{
	cr->set_source_rgb(bg.r, bg.g, bg.b);
	create_path(cr, x, y, 0, 4);
	cr->fill();

	cr->set_source_rgb(tl_shadow.r, tl_shadow.g, tl_shadow.b);
	create_path(cr, x, y, 0, 2);
	cr->stroke();

	cr->set_source_rgb(br_shadow.r, br_shadow.g, br_shadow.b);
	create_path(cr, x, y, 2, 4);
	cr->stroke();
}

inline void
render_cf(Cairo::RefPtr<Cairo::Context> cr, int /*note*/, double x[], double y[],
		PianoRollHeader::Color& bg, PianoRollHeader::Color& tl_shadow, PianoRollHeader::Color& br_shadow)
{
	cr->set_source_rgb(bg.r, bg.g, bg.b);
	create_path(cr, x, y, 0, 6);
	cr->fill();

	cr->set_source_rgb(tl_shadow.r, tl_shadow.g, tl_shadow.b);
	create_path(cr, x, y, 0, 4);
	cr->stroke();

	cr->set_source_rgb(br_shadow.r, br_shadow.g, br_shadow.b);
	create_path(cr, x, y, 4, 6);
	cr->stroke();
}

inline void
render_eb(Cairo::RefPtr<Cairo::Context> cr, int /*note*/, double x[], double y[],
		PianoRollHeader::Color& bg, PianoRollHeader::Color& tl_shadow, PianoRollHeader::Color& br_shadow)
{
	cr->set_source_rgb(bg.r, bg.g, bg.b);
	create_path(cr, x, y, 0, 6);
	cr->fill();

	cr->set_source_rgb(tl_shadow.r, tl_shadow.g, tl_shadow.b);
	create_path(cr, x, y, 0, 2);
	cr->stroke();
	create_path(cr, x, y, 4, 5);
	cr->stroke();

	cr->set_source_rgb(br_shadow.r, br_shadow.g, br_shadow.b);
	create_path(cr, x, y, 2, 4);
	cr->stroke();
	create_path(cr, x, y, 5, 6);
	cr->stroke();
}

inline void
render_dga(Cairo::RefPtr<Cairo::Context> cr, int /*note*/, double x[], double y[],
		 PianoRollHeader::Color& bg, PianoRollHeader::Color& tl_shadow, PianoRollHeader::Color& br_shadow)
{
	cr->set_source_rgb(bg.r, bg.g, bg.b);
	create_path(cr, x, y, 0, 8);
	cr->fill();

	cr->set_source_rgb(tl_shadow.r, tl_shadow.g, tl_shadow.b);
	create_path(cr, x, y, 0, 4);
	cr->stroke();
	create_path(cr, x, y, 6, 7);
	cr->stroke();

	cr->set_source_rgb(br_shadow.r, br_shadow.g, br_shadow.b);
	create_path(cr, x, y, 4, 6);
	cr->stroke();
	create_path(cr, x, y, 7, 8);
	cr->stroke();
}

void
PianoRollHeader::get_path(PianoRollHeader::ItemType note_type, int note, double x[], double y[])
{
	double y_pos = floor(_view.note_to_y(note)) - 0.5f;
	double note_height;
	double other_y1 = floor(_view.note_to_y(note+1)) + floor(_note_height / 2.0f) + 0.5f;
	double other_y2 = floor(_view.note_to_y(note-1)) + floor(_note_height / 2.0f) - 1.0f;
	double width = get_width();

	if (note == 0) {
		note_height = floor(_view.contents_height()) - y_pos;
	} else {
		note_height = floor(_view.note_to_y(note - 1)) - y_pos;
	}

	switch (note_type) {
	case BLACK_SEPARATOR:
		x[0] = 1.5f;
		y[0] = y_pos;
		x[1] = _black_note_width;
		y[1] = y_pos;
		break;
	case BLACK_MIDDLE_SEPARATOR:
		x[0] = _black_note_width;
		y[0] = y_pos + floor(_note_height / 2.0f);
		x[1] = width - 1.0f;
		y[1] = y[0];
		break;
	case BLACK:
		x[0] = 1.5f;
		y[0] = y_pos + note_height - 0.5f;
		x[1] = 1.5f;
		y[1] = y_pos + 1.0f;
		x[2] = _black_note_width;
		y[2] = y_pos + 1.0f;
		x[3] = _black_note_width;
		y[3] = y_pos + note_height - 0.5f;
		x[4] = 1.5f;
		y[4] = y_pos + note_height - 0.5f;
		return;
	case WHITE_SEPARATOR:
		x[0] = 1.5f;
		y[0] = y_pos;
		x[1] = width - 1.5f;
		y[1] = y_pos;
		break;
	case WHITE_RECT:
		x[0] = 1.5f;
		y[0] = y_pos + note_height - 0.5f;
		x[1] = 1.5f;
		y[1] = y_pos + 1.0f;
		x[2] = width - 1.5f;
		y[2] = y_pos + 1.0f;
		x[3] = width - 1.5f;
		y[3] = y_pos + note_height - 0.5f;
		x[4] = 1.5f;
		y[4] = y_pos + note_height - 0.5f;
		return;
	case WHITE_CF:
		x[0] = 1.5f;
		y[0] = y_pos + note_height - 1.5f;
		x[1] = 1.5f;
		y[1] = y_pos + 1.0f;
		x[2] = _black_note_width + 1.0f;
		y[2] = y_pos + 1.0f;
		x[3] = _black_note_width + 1.0f;
		y[3] = other_y1;
		x[4] = width - 1.5f;
		y[4] = other_y1;
		x[5] = width - 1.5f;
		y[5] = y_pos + note_height - 1.5f;
		x[6] = 1.5f;
		y[6] = y_pos + note_height - 1.5f;
		return;
	case WHITE_EB:
		x[0] = 1.5f;
		y[0] = y_pos + note_height - 1.5f;
		x[1] = 1.5f;
		y[1] = y_pos + 1.0f;
		x[2] = width - 1.5f;
		y[2] = y_pos + 1.0f;
		x[3] = width - 1.5f;
		y[3] = other_y2;
		x[4] = _black_note_width + 1.0f;
		y[4] = other_y2;
		x[5] = _black_note_width + 1.0f;
		y[5] = y_pos + note_height - 1.5f;
		x[6] = 1.5f;
		y[6] = y_pos + note_height - 1.5f;
		return;
	case WHITE_DGA:
		x[0] = 1.5f;
		y[0] = y_pos + note_height - 1.5f;
		x[1] = 1.5f;
		y[1] = y_pos + 1.0f;
		x[2] = _black_note_width + 1.0f;
		y[2] = y_pos + 1.0f;
		x[3] = _black_note_width + 1.0f;
		y[3] = other_y1;
		x[4] = width - 1.5f;
		y[4] = other_y1;
		x[5] = width - 1.5f;
		y[5] = other_y2;
		x[6] = _black_note_width + 1.0f;
		y[6] = other_y2;
		x[7] = _black_note_width + 1.0f;
		y[7] = y_pos + note_height - 1.5f;
		x[8] = 1.5f;
		y[8] = y_pos + note_height - 1.5f;
		return;
	default:
		return;
	}
}

bool
PianoRollHeader::on_expose_event (GdkEventExpose* ev)
{
	GdkRectangle& rect = ev->area;
	double font_size;
	int lowest, highest;
	Cairo::RefPtr<Cairo::Context> cr = get_window()->create_cairo_context();
	Cairo::RefPtr<Cairo::LinearGradient> pat = Cairo::LinearGradient::create(0, 0, _black_note_width, 0);
	double x[9];
	double y[9];
	Color bg, tl_shadow, br_shadow;
	int oct_rel;
	int y1 = max(rect.y, 0);
	int y2 = min(rect.y + rect.height, (int) floor(_view.contents_height() - 1.0f));

	//Cairo::TextExtents te;
	lowest = max(_view.lowest_note(), _view.y_to_note(y2));
	highest = min(_view.highest_note(), _view.y_to_note(y1));

	if (lowest > 127) {
		lowest = 0;
	}

	cr->select_font_face ("Georgia", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
	font_size = min((double) 10.0f, _note_height - 4.0f);
	cr->set_font_size(font_size);

	/* fill the entire rect with the color for non-highlighted white notes.
	 * then we won't have to draw the background for those notes,
	 * and would only have to draw the background for the one highlighted white note*/
	//cr->rectangle(rect.x, rect.y, rect.width, rect.height);
	//cr->set_source_rgb(white.r, white.g, white.b);
	//cr->fill();

	cr->set_line_width(1.0f);

	/* draw vertical lines with shade at both ends of the widget */
	cr->set_source_rgb(0.0f, 0.0f, 0.0f);
	cr->move_to(0.5f, rect.y);
	cr->line_to(0.5f, rect.y + rect.height);
	cr->stroke();
	cr->move_to(get_width() - 0.5f, rect.y);
	cr->line_to(get_width() - 0.5f, rect.y + rect.height);
	cr->stroke();

	//pat->add_color_stop_rgb(0.0, 0.33, 0.33, 0.33);
	//pat->add_color_stop_rgb(0.2, 0.39, 0.39, 0.39);
	//pat->add_color_stop_rgb(1.0, 0.22, 0.22, 0.22);
	//cr->set_source(pat);

	for (int i = lowest; i <= highest; ++i) {
		oct_rel = i % 12;

		switch (oct_rel) {
		case 1:
		case 3:
		case 6:
		case 8:
		case 10:
			/* black note */
			if (i == _highlighted_note) {
				bg.set(black_highlight);
			} else {
				bg.set(black);
			}

			if (_active_notes[i]) {
				tl_shadow.set(black_shade_dark);
				br_shadow.set(black_shade_light);
			} else {
				tl_shadow.set(black_shade_light);
				br_shadow.set(black_shade_dark);
			}

			/* draw black separators */
			cr->set_source_rgb(0.0f, 0.0f, 0.0f);
			get_path(BLACK_SEPARATOR, i, x, y);
			create_path(cr, x, y, 0, 1);
			cr->stroke();

			get_path(BLACK_MIDDLE_SEPARATOR, i, x, y);
			create_path(cr, x, y, 0, 1);
			cr->stroke();

			get_path(BLACK, i, x, y);
			render_rect(cr, i, x, y, bg, tl_shadow, br_shadow);
			break;

		default:
			/* white note */
			if (i == _highlighted_note) {
				bg.set(white_highlight);
			} else {
				bg.set(white);
			}

			if (_active_notes[i]) {
				tl_shadow.set(white_shade_dark);
				br_shadow.set(white_shade_light);
			} else {
				tl_shadow.set(white_shade_light);
				br_shadow.set(white_shade_dark);
			}

			switch(oct_rel) {
			case 0:
			case 5:
				if (i == _view.highest_note()) {
					get_path(WHITE_RECT, i, x, y);
					render_rect(cr, i, x, y, bg, tl_shadow, br_shadow);
				} else {
					get_path(WHITE_CF, i, x, y);
					render_cf(cr, i, x, y, bg, tl_shadow, br_shadow);
				}
				break;

			case 2:
			case 7:
			case 9:
				if (i == _view.highest_note()) {
					get_path(WHITE_EB, i, x, y);
					render_eb(cr, i, x, y, bg, tl_shadow, br_shadow);
				} else if (i == _view.lowest_note()) {
					get_path(WHITE_CF, i, x, y);
					render_cf(cr, i, x, y, bg, tl_shadow, br_shadow);
				} else {
					get_path(WHITE_DGA, i, x, y);
					render_dga(cr, i, x, y, bg, tl_shadow, br_shadow);
				}
				break;

			case 4:
			case 11:
				cr->set_source_rgb(0.0f, 0.0f, 0.0f);
				get_path(WHITE_SEPARATOR, i, x, y);
				create_path(cr, x, y, 0, 1);
				cr->stroke();

				if (i == _view.lowest_note()) {
					get_path(WHITE_RECT, i, x, y);
					render_rect(cr, i, x, y, bg, tl_shadow, br_shadow);
				} else {
					get_path(WHITE_EB, i, x, y);
					render_eb(cr, i, x, y, bg, tl_shadow, br_shadow);
				}
				break;

			default:
				break;

			}
			break;

		}

		/* render the name of which C this is */
		if (oct_rel == 0) {
			std::stringstream s;
			double y = floor(_view.note_to_y(i)) - 0.5f;
			double note_height = floor(_view.note_to_y(i - 1)) - y;

			int cn = i / 12 - 1;
			s << "C" << cn;

			//cr->get_text_extents(s.str(), te);
			cr->set_source_rgb(0.30f, 0.30f, 0.30f);
			cr->move_to(2.0f, y + note_height - 1.0f - (note_height - font_size) / 2.0f);
			cr->show_text(s.str());
		}
	}

	return true;
}

bool
PianoRollHeader::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_dragging) {

		int note = _view.y_to_note(ev->y);

		if (editor().current_mouse_mode() == Editing::MouseRange) {
			
			/* select note range */

			if (Keyboard::no_modifiers_active (ev->state)) {
				AddNoteSelection (note); // EMIT SIGNAL
			}

		} else {
			
			/* play notes */

			if (_highlighted_note != no_note) {
				if (note > _highlighted_note) {
					invalidate_note_range(_highlighted_note, note);
				} else {
					invalidate_note_range(note, _highlighted_note);
				}
				
				_highlighted_note = note;
			}
			
			/* redraw already taken care of above */
			if (_clicked_note != no_note && _clicked_note != note) {
				_active_notes[_clicked_note] = false;
				send_note_off(_clicked_note);
				
				_clicked_note = note;
				
				if (!_active_notes[note]) {
					_active_notes[note] = true;
					send_note_on(note);
				}
			}
		}
	}

	//win->process_updates(false);

	return true;
}

bool
PianoRollHeader::on_button_press_event (GdkEventButton* ev)
{
	int note = _view.y_to_note(ev->y);

	if (ev->button == 2 && ev->type == GDK_BUTTON_PRESS) {
		if (Keyboard::no_modifiers_active (ev->state)) {
			SetNoteSelection (note); // EMIT SIGNAL
			return true;
		}
		return false;
	}
	
	if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS && note >= 0 && note < 128) {
		
		add_modal_grab();
		_dragging = true;
		
		if (!_active_notes[note]) {
			_active_notes[note] = true;
			_clicked_note = note;
			send_note_on(note);
			
			invalidate_note_range(note, note);
		} else {
			reset_clicked_note(note);
		}
	}

	return true;
}

bool
PianoRollHeader::on_button_release_event (GdkEventButton* ev)
{
	int note = _view.y_to_note(ev->y);

	if (editor().current_mouse_mode() == Editing::MouseRange) {

		if (Keyboard::no_modifiers_active (ev->state)) {
			AddNoteSelection (note); // EMIT SIGNAL
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			ToggleNoteSelection (note); // EMIT SIGNAL
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::RangeSelectModifier)) {
			ExtendNoteSelection (note); // EMIT SIGNAL
		}
		
	} else {

		if (_dragging) {
			remove_modal_grab();

			if (note == _clicked_note) {
				reset_clicked_note(note);
			}
		}
	}

	_dragging = false;
	return true;
}

bool
PianoRollHeader::on_enter_notify_event (GdkEventCrossing* ev)
{
	_highlighted_note = _view.y_to_note(ev->y);

	invalidate_note_range(_highlighted_note, _highlighted_note);
	return true;
}

bool
PianoRollHeader::on_leave_notify_event (GdkEventCrossing*)
{
	invalidate_note_range(_highlighted_note, _highlighted_note);

	if (_clicked_note != no_note) {
		reset_clicked_note(_clicked_note, _clicked_note != _highlighted_note);
	}

	_highlighted_note = no_note;
	return true;
}

bool
PianoRollHeader::on_scroll_event (GdkEventScroll*)
{
	return true;
}

void
PianoRollHeader::note_range_changed()
{
	_note_height = floor(_view.note_height()) + 0.5f;
	queue_draw();
}

void
PianoRollHeader::invalidate_note_range(int lowest, int highest)
{
	Glib::RefPtr<Gdk::Window> win = get_window();
	Gdk::Rectangle rect;

	// the non-rectangular geometry of some of the notes requires more
	// redraws than the notes that actually changed.
	switch(lowest % 12) {
	case 0:
	case 5:
		lowest = max((int) _view.lowest_note(), lowest);
		break;
	default:
		lowest = max((int) _view.lowest_note(), lowest - 1);
		break;
	}

	switch(highest % 12) {
	case 4:
	case 11:
		highest = min((int) _view.highest_note(), highest);
		break;
	case 1:
	case 3:
	case 6:
	case 8:
	case 10:
		highest = min((int) _view.highest_note(), highest + 1);
		break;
	default:
		highest = min((int) _view.highest_note(), highest + 2);
		break;
	}

	double y = _view.note_to_y(highest);
	double height = _view.note_to_y(lowest - 1) - y;

	rect.set_x(0);
	rect.set_width(get_width());
	rect.set_y((int) floor(y));
	rect.set_height((int) floor(height));

	if (win) {
		win->invalidate_rect(rect, false);
	}
}

void
PianoRollHeader::on_size_request(Gtk::Requisition* r)
{
	r->width = 20;
}

void
PianoRollHeader::on_size_allocate(Gtk::Allocation& a)
{
	DrawingArea::on_size_allocate(a);

	_black_note_width = floor(0.7 * get_width()) + 0.5f;
}

void
PianoRollHeader::send_note_on(uint8_t note)
{
	boost::shared_ptr<ARDOUR::MidiTrack> track = _view.trackview().midi_track();
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*> (&_view.trackview ());

	//cerr << "note on: " << (int) note << endl;

	if (track) {
		_event[0] = (MIDI_CMD_NOTE_ON | mtv->get_channel_for_add ());
		_event[1] = note;
		_event[2] = 100;

		track->write_immediate_event(3, _event);
	}
}

void
PianoRollHeader::send_note_off(uint8_t note)
{
	boost::shared_ptr<ARDOUR::MidiTrack> track = _view.trackview().midi_track();
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*> (&_view.trackview ());

	if (track) {
		_event[0] = (MIDI_CMD_NOTE_OFF | mtv->get_channel_for_add ());
		_event[1] = note;
		_event[2] = 100;

		track->write_immediate_event(3, _event);
	}
}

void
PianoRollHeader::reset_clicked_note (uint8_t note, bool invalidate)
{
	_active_notes[note] = false;
	_clicked_note = no_note;
	send_note_off (note);
	if (invalidate) {
		invalidate_note_range (note, note);
	}
}

PublicEditor&
PianoRollHeader::editor() const
{
	return _view.trackview().editor();
}
