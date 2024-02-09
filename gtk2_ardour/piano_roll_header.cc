/*
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/midi_track.h"
#include "evoral/midi_events.h"
#include <iostream>

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gtk_ui.h"

#include "editing.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "piano_roll_header.h"
#include "public_editor.h"
#include "ui_config.h"
#include "midi++/midnam_patch.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtkmm2ext;

PianoRollHeader::PianoRollHeader(MidiStreamView& v)
	: have_note_names (false)
	, _adj(v.note_range_adjustment)
	, _view(v)
	, _font_descript ("Sans Bold")
	, _font_descript_big_c ("Sans")
	, _font_descript_midnam ("Sans")
	, _highlighted_note (NO_MIDI_NOTE)
	, _clicked_note (NO_MIDI_NOTE)
	, _dragging (false)
	, _scroomer_size (63.f)
	, _scroomer_drag (false)
	, _old_y (0.0)
	, _fract (0.0)
	, _scroomer_state (NONE)
	, _scroomer_button_state (NONE)
	, _saved_top_val (0.0)
	, _saved_bottom_val (127.0)
	, _mini_map_display (false)
	, entered (false)
{
	_layout = Pango::Layout::create (get_pango_context());
	_big_c_layout = Pango::Layout::create (get_pango_context());
	_font_descript_big_c.set_absolute_size (10.0 * Pango::SCALE);
	_big_c_layout->set_font_description(_font_descript_big_c);
	_midnam_layout = Pango::Layout::create (get_pango_context());

	_adj.set_lower(0);
	_adj.set_upper(127);

	/* set minimum view range to one octave */
	//set_min_page_size(12);

	//_adj = v->note_range_adjustment;

	Gtkmm2ext::UI::instance()->set_tip (*this, string_compose (_("Left-button to play a note, left-button-drag to play a series of notes\n"
	                                                             "%1-left-button to select or extend selection to all notes with this pitch\n"),
	                                                           Keyboard::tertiary_modifier_name()));
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
create_path (Cairo::RefPtr<Cairo::Context> cr, double x[], double y[], int start, int stop)
{
	cr->move_to (x[start], y[start]);

	for (int i = start + 1; i <= stop; ++i) {
		cr->line_to (x[i], y[i]);
	}
}

inline void
render_rect(Cairo::RefPtr<Cairo::Context> cr, int note, double x[], double y[],
	     Gtkmm2ext::Color& bg)
{
	set_source_rgba(cr, bg);
	create_path(cr, x, y, 0, 4);
	cr->fill();
}

void
PianoRollHeader::render_scroomer(Cairo::RefPtr<Cairo::Context> cr)
{
	double scroomer_top = max (1.0, (1.0 - ((_adj.get_value()+_adj.get_page_size()) / 127.0)) * get_height () );
	double scroomer_bottom = (1.0 - (_adj.get_value () / 127.0)) * get_height ();
	double scroomer_width = _scroomer_size;

	Gtkmm2ext::Color c = UIConfiguration::instance().color_mod (X_("scroomer"), X_("scroomer alpha"));
	Gtkmm2ext::Color save_color (c);

	if (entered) {
		c = HSV (c).lighter (0.25).color();
	}

	set_source_rgba (cr, c);
	cr->move_to (1.f, scroomer_top);
	cr->line_to (scroomer_width - 1.f, scroomer_top);
	cr->line_to (scroomer_width - 1.f, scroomer_bottom);
	cr->line_to (1.f, scroomer_bottom);
	cr->line_to (1.f, scroomer_top);
	cr->fill();

	if (entered) {
		cr->save ();
		c = HSV (save_color).lighter (0.9).color();
		set_source_rgba (cr, c);
		cr->set_line_width (4.);
		cr->move_to (1.f, scroomer_top + 2.);
		cr->line_to (scroomer_width - 1.f, scroomer_top + 2.);
		cr->stroke ();
		cr->line_to (scroomer_width - 1.f, scroomer_bottom - 2.);
		cr->line_to (2.f, scroomer_bottom - 2.);
		cr->stroke ();
		cr->restore ();
	}
}

bool
PianoRollHeader::on_scroll_event (GdkEventScroll* ev)
{
	int note_range = _adj.get_page_size ();
	int note_lower = _adj.get_value ();

	if(ev->state == GDK_SHIFT_MASK){
		switch (ev->direction) {
		case GDK_SCROLL_UP: //ZOOM IN
			_view.apply_note_range (min(note_lower + 1, 127), max(note_lower + note_range - 1,0), true);
			break;
		case GDK_SCROLL_DOWN: //ZOOM OUT
			_view.apply_note_range (max(note_lower - 1,0), min(note_lower + note_range + 1, 127), true);
			break;
		default:
			return false;
		}
	}else{
		switch (ev->direction) {
		case GDK_SCROLL_UP:
			_adj.set_value (min (note_lower + 1, 127 - note_range));
			break;
		case GDK_SCROLL_DOWN:
			_adj.set_value (note_lower - 1.0);
			break;
		default:
			return false;
		}
	}

	set_note_highlight (_view.y_to_note (ev->y));

	_adj.value_changed ();
	queue_draw ();
	return true;
}


void
PianoRollHeader::get_path (int note, double x[], double y[])
{
	double scroomer_size = _scroomer_size;
	double y_pos = floor(_view.note_to_y(note));
	double note_height;
	_raw_note_height = floor(_view.note_to_y(note - 1)) - y_pos;
	double width = get_width() - 1.0f;

	if (note == 0) {
		note_height = floor(_view.contents_height()) - y_pos;
	} else {
		note_height = _raw_note_height <= 3 ? _raw_note_height : _raw_note_height - 1.f;
	}

	x[0] = scroomer_size;
	y[0] = y_pos + note_height;

	x[1] = scroomer_size;
	y[1] = y_pos;

	x[2] = width;
	y[2] = y_pos;

	x[3] = width;
	y[3] = y_pos + note_height;

	x[4] = scroomer_size;
	y[4] = y_pos + note_height;
	return;
}

bool
PianoRollHeader::on_expose_event (GdkEventExpose* ev)
{
	GdkRectangle& rect = ev->area;
	int lowest, highest;
	Gtkmm2ext::Color bg;
	Cairo::RefPtr<Cairo::Context> cr = get_window()->create_cairo_context();
	double x[9];
	double y[9];
	int oct_rel;
	int y1 = max(rect.y, 0);
	int y2 = min(rect.y + rect.height, (int) floor(_view.contents_height()));
	double av_note_height = get_height () / _adj.get_page_size ();
	int bc_height, bc_width;

	//Reduce the frequency of Pango layout resizing
	//if (int(_old_av_note_height) != int(av_note_height)) {
	//Set Pango layout keyboard c's size
	_font_descript.set_absolute_size (av_note_height * 0.7 * Pango::SCALE);
	_layout->set_font_description(_font_descript);

	//change mode of midnam display
	if (av_note_height >= 8.0) {
		_mini_map_display = false;
	} else {
		_mini_map_display = true;
	}

	//Set Pango layout midnam size
	_font_descript_midnam.set_absolute_size (max(8.0 * 0.7 * Pango::SCALE, (int)av_note_height * 0.7 * Pango::SCALE));

	_midnam_layout->set_font_description(_font_descript_midnam);

	lowest = max(_view.lowest_note(), _view.y_to_note(y2));
	highest = min(_view.highest_note(), _view.y_to_note(y1));

	if (lowest > 127) {
		lowest = 0;
	}

	/* fill the entire rect with the color for non-highlighted white notes.
	 * then we won't have to draw the background for those notes,
	 * and would only have to draw the background for the one highlighted white note*/
	//cr->rectangle(rect.x, rect.y, rect.width, rect.height);
	//r->set_source_rgb(1, 0,0);
	//cr->fill();

	cr->set_line_width (1.0f);

	Gtkmm2ext::Color white           = UIConfiguration::instance().color (X_("piano key white"));
	Gtkmm2ext::Color white_highlight = UIConfiguration::instance().color (X_("piano key highlight"));
	Gtkmm2ext::Color black           = UIConfiguration::instance().color (X_("piano key black"));
	Gtkmm2ext::Color black_highlight = UIConfiguration::instance().color (X_("piano key highlight"));
	Gtkmm2ext::Color textc           = UIConfiguration::instance().color (X_("gtk_foreground"));

	/* draw vertical lines on both sides of the widget */
	cr->set_source_rgb(0.0f, 0.0f, 0.0f);
	cr->move_to(0.f, rect.y);
	cr->line_to(0.f, rect.y + rect.height);
	cr->stroke();
	cr->move_to(get_width(),rect.y);
	cr->line_to(get_width(), rect.y + get_height ());
	cr->stroke();

	// Render the MIDNAM text or its equivalent.  First, set up a clip
	// region so that the text doesn't spill, regardless of its length.

	cr->save();

	cr->rectangle (0,0,_scroomer_size, get_height () );
	cr->clip();

	if (show_scroomer()) {

		/* Draw the actual text */

		for (int i = lowest; i <= highest; ++i) {
			int size_x, size_y;
			double y = floor(_view.note_to_y(i)) - 0.5f;
			NoteName & note (note_names[i]);

			_midnam_layout->set_text (note.name);

			set_source_rgba(cr, textc);
			cr->move_to(2.f, y);

			if (!_mini_map_display) {
				_midnam_layout->show_in_cairo_context (cr);
			} else {
				/* Too small for text, just show a thing rect where the
				   text would have been.
				*/
				if (!note.from_midnam) {
					set_source_rgba(cr, textc);
				}
				pango_layout_get_pixel_size (_midnam_layout->gobj (), &size_x, &size_y);
				cr->rectangle (2.f, y + (av_note_height * 0.5), size_x, av_note_height * 0.2);
				cr->fill ();
			}
		}

		/* Add a gradient over the text, to act as a sort of "visual
		   elision". This avoids using text elision with "..." which takes up too
		   much space.
		*/
		Gtkmm2ext::Color bg = UIConfiguration::instance().color (X_("gtk_background"));
		double r,g,b,a;
		Gtkmm2ext::color_to_rgba(bg,r,g,b,a);
		double fade_width = 30.;
		auto gradient_ptr = Cairo::LinearGradient::create (_scroomer_size - fade_width, 0, _scroomer_size, 0);
		gradient_ptr->add_color_stop_rgba (0,r,g,b,0);
		gradient_ptr->add_color_stop_rgba (1,r,g,b,1);
		cr->set_source (gradient_ptr);
		cr->rectangle (_scroomer_size - fade_width, 0, _scroomer_size, get_height () );
		cr->fill();
	}

	/* Now draw the semi-transparent scroomer over the top */

	render_scroomer(cr);

	/* Done with clip region */

	cr->restore();

	/* Now draw black/white rects for each note, following standard piano
	   layout, but without a setback/offset for the black keys
	*/

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
				bg = black_highlight;
			} else {
				bg = black;
			}

			/* draw black separators */
			cr->set_source_rgb (0.0f, 0.0f, 0.0f);
			get_path (i, x, y);
			create_path (cr, x, y, 0, 1);
			cr->stroke();

			get_path (i, x, y);
			create_path (cr, x, y, 0, 1);
			cr->stroke();

			get_path (i, x, y);
			render_rect (cr, i, x, y, bg);
			break;
		}

		switch(oct_rel) {
		case 0:
		case 2:
		case 4:
		case 5:
		case 7:
		case 9:
		case 11:
			if (i == _highlighted_note) {
				bg = white_highlight;
			} else {
				bg  = white;
			}
			get_path (i, x, y);
			render_rect (cr, i, x, y, bg);
			break;
		default:
			break;

		}
	}

	/* render the C<N> of the key, when key is too small to contain text we
	   place the C<N> on the midnam scroomer area.

	   we render an additional 5 notes below the lowest note displayed
	   so that the top of the C is shown to maintain visual context
	 */
	for (int i = lowest - 5; i <= highest; ++i) {
		double y = floor(_view.note_to_y(i)) - 0.5f;
		double note_height = i == 0? av_note_height : floor(_view.note_to_y(i - 1)) - y;
		oct_rel = i % 12;

		if (oct_rel == 0 || (oct_rel == 7 && _adj.get_page_size() <=10) ) {
			std::stringstream s;

			int cn = i / 12 - 1;

			if (oct_rel == 0){
				s << "C" << cn;
			}else{
				s << "G" << cn;
			}

			if (av_note_height > 12.0){
				set_source_rgba(cr, black);
				_layout->set_text (s.str());
				cr->move_to(_scroomer_size, ceil(y+1.));
				_layout->show_in_cairo_context (cr);
			}else{
				set_source_rgba(cr, textc);
				_big_c_layout->set_text (s.str());
				pango_layout_get_pixel_size (_big_c_layout->gobj(), &bc_width, &bc_height);
				cr->move_to(_scroomer_size - 18, y - bc_height + av_note_height);
				_big_c_layout->show_in_cairo_context (cr);
				cr->move_to(_scroomer_size - 18, y + note_height);
				cr->line_to(_scroomer_size, y + note_height);
				cr->stroke();
			}
		}
	}

	return true;
}

void
PianoRollHeader::instrument_info_change ()
{
	have_note_names = false;

	for (int i = 0; i < 128; ++i) {
		note_names[i] = get_note_name (i);

		if (note_names[i].from_midnam) {
			have_note_names = true;
		}
	}

	queue_resize ();

	/* need this to get editor to potentially sync all
	   track header widths if our piano roll header changes
	   width.
	*/

	_view.trackview().stripable()->gui_changed ("visible_tracks", (void *) 0); /* EMIT SIGNAL */
}

PianoRollHeader::NoteName
PianoRollHeader::get_note_name (int note)
{
	using namespace MIDI::Name;
	std::string name;
	std::string note_n;
	NoteName rtn;

	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&_view.trackview());

	if (mtv) {
		string chn = mtv->gui_property (X_("midnam-channel"));

		if (!chn.empty()) {

			int midnam_channel;

			sscanf (chn.c_str(), "%*s %d", &midnam_channel);
			midnam_channel--;

			name = mtv->route()->instrument_info ().get_note_name (
				0,               //bank
				0,               //program
				midnam_channel,  //channel
				note);           //note
		}
	}

	int oct_rel = note % 12;
	switch(oct_rel) {
		case 0:
			note_n = "C";
			break;
		case 1:
			note_n = "C♯";
			break;
		case 2:
			note_n = "D";
			break;
		case 3:
			note_n = "D♯";
			break;
		case 4:
			note_n = "E";
			break;
		case 5:
			note_n = "F";
			break;
		case 6:
			note_n = "F♯";
			break;
		case 7:
			note_n = "G";
			break;
		case 8:
			note_n = "G♯";
			break;
		case 9:
			note_n = "A";
			break;
		case 10:
			note_n = "A♯";
			break;
		case 11:
			note_n = "B";
			break;
		default:
			break;
	}

	std::string new_string = std::string(3 - std::to_string(note).length(), '0') + std::to_string(note);
	rtn.name = name.empty()? new_string + " " + note_n : name;
	rtn.from_midnam = !name.empty();
	return rtn;
}

bool
PianoRollHeader::on_motion_notify_event (GdkEventMotion* ev)
{
	if (!_scroomer_drag && ev->x < _scroomer_size){
		Gdk::Cursor m_Cursor;
		double scroomer_top = max(1.0, (1.0 - ((_adj.get_value()+_adj.get_page_size()) / 127.0)) * get_height () );
		double scroomer_bottom = (1.0 - (_adj.get_value () / 127.0)) * get_height ();
		if (ev->y > scroomer_top - 5 && ev->y < scroomer_top + 5){
			m_Cursor = Gdk::Cursor (Gdk::TOP_SIDE);
			get_window()->set_cursor(m_Cursor);
			_scroomer_state = TOP;
		}else if (ev->y > scroomer_bottom - 5 && ev->y < scroomer_bottom + 5){
			m_Cursor = Gdk::Cursor (Gdk::BOTTOM_SIDE);
			get_window()->set_cursor(m_Cursor);
			_scroomer_state = BOTTOM;
		}else {
			_scroomer_state = MOVE;
			get_window()->set_cursor();
		}
	}

	if (_scroomer_drag){
		double pixel2val = 127.0 / get_height();
		double delta = _old_y - ev->y;
		double val_at_pointer = (delta * pixel2val);
		double real_val_at_pointer = 127.0 - (ev->y * pixel2val);
		double note_range = _adj.get_page_size ();

		switch (_scroomer_button_state){
			case MOVE:
				_fract += val_at_pointer;
				_fract = (_fract + note_range > 127.0)? 127.0 - note_range : _fract;
				_fract = max(0.0, _fract);
				_adj.set_value (min(_fract, 127.0 - note_range));
				break;
			case TOP:
				real_val_at_pointer = real_val_at_pointer <= _saved_top_val? _adj.get_value() + _adj.get_page_size() : real_val_at_pointer;
				real_val_at_pointer = min(127.0, real_val_at_pointer);
				if (_note_height >= UIConfiguration::instance().get_max_note_height()){
					_saved_top_val  = min(_adj.get_value() + _adj.get_page_size (), 127.0);
				} else {
					_saved_top_val = 0.0;
				}
				//if we are at largest note size & the user is moving down don't do anything
				//FIXME we are using a heuristic of 18.5 for max note size, but this changes when track size is small to 19.5?
				_view.apply_note_range (_adj.get_value (), real_val_at_pointer, true);
				break;
			case BOTTOM:
				real_val_at_pointer = max(0.0, real_val_at_pointer);
				real_val_at_pointer = real_val_at_pointer >= _saved_bottom_val? _adj.get_value() : real_val_at_pointer;
				if (_note_height >= UIConfiguration::instance().get_max_note_height()){
					_saved_bottom_val  = _adj.get_value();
				} else {
					_saved_bottom_val = 127.0;
				}
				_view.apply_note_range (real_val_at_pointer, _adj.get_value () + _adj.get_page_size (), true);
				break;
			default:
				break;
		}
	}else{
		int note = _view.y_to_note(ev->y);
		set_note_highlight (note);

		if (_dragging) {

			if ( false /*editor().current_mouse_mode() == Editing::MouseRange*/ ) {   //ToDo:  fix this.  this mode is buggy, and of questionable utility anyway

				/* select note range */

				if (Keyboard::no_modifiers_active (ev->state)) {
					AddNoteSelection (note); // EMIT SIGNAL
				}

			} else {
				/* play notes */
				/* redraw already taken care of above in set_note_highlight */
				if (_clicked_note != NO_MIDI_NOTE && _clicked_note != note) {
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
	}
	_adj.value_changed ();
	queue_draw ();
	_old_y = ev->y;
	//win->process_updates(false);

	return true;
}

bool
PianoRollHeader::on_button_press_event (GdkEventButton* ev)
{
	_scroomer_button_state = _scroomer_state;

	if (ev->button == 1 && ev->x <= _scroomer_size){

		if (ev->type == GDK_2BUTTON_PRESS) {
			MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&_view.trackview());
			if (mtv) {
				mtv->set_visibility_note_range (MidiStreamView::ContentsRange, false);
			}
			return true;
		}

		_scroomer_drag = true;
		_old_y = ev->y;
		_fract = _adj.get_value();
		_fract_top = _adj.get_value() + _adj.get_page_size();
		return true;

	} else {
		int note = _view.y_to_note(ev->y);
		bool tertiary = Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier);
		bool primary = Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier);

		if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {
			if (primary) {
				_adj.set_value (0.0);
				_adj.set_page_size (127.0);
				_adj.value_changed ();
				queue_draw ();
			}
			return true;
		} else if (ev->button == 2 && Keyboard::no_modifiers_active (ev->state)) {
			SetNoteSelection (note); // EMIT SIGNAL
			return true;
		} else if (tertiary && (ev->button == 1 || ev->button == 2)) {
			ExtendNoteSelection (note); // EMIT SIGNAL
			return true;
		} else if (primary && (ev->button == 1 || ev->button == 2)) {
			ToggleNoteSelection (note); // EMIT SIGNAL
			return true;
		} else if (ev->button == 1 && note >= 0 && note < 128) {
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
	}
	return true;
}

bool
PianoRollHeader::on_button_release_event (GdkEventButton* ev)
{
	if (_scroomer_drag){
		_scroomer_drag = false;
	}
	int note = _view.y_to_note(ev->y);

	if (false /*editor().current_mouse_mode() == Editing::MouseRange*/) { //Todo:  this mode is buggy, and of questionable utility anyway

		if (Keyboard::no_modifiers_active (ev->state)) {
			AddNoteSelection (note); // EMIT SIGNAL
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			ToggleNoteSelection (note); // EMIT SIGNAL
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::RangeSelectModifier)) {
			ExtendNoteSelection (note); // EMIT SIGNAL
		}

	} else {
		if (_dragging) {
			remove_modal_grab ();

			if (note == _clicked_note) {
				reset_clicked_note (note);
			}
		}
	}

	_dragging = false;
	return true;
}

void
PianoRollHeader::set_note_highlight (uint8_t note)
{
	if (_highlighted_note == note) {
		return;
	}

	if (_highlighted_note != NO_MIDI_NOTE) {
		if (note > _highlighted_note) {
			invalidate_note_range (_highlighted_note, note);
		} else {
			invalidate_note_range (note, _highlighted_note);
		}
	}

	_highlighted_note = note;

	if (_highlighted_note != NO_MIDI_NOTE) {
		invalidate_note_range (_highlighted_note, _highlighted_note);
	}
}

bool
PianoRollHeader::on_enter_notify_event (GdkEventCrossing* ev)
{
	set_note_highlight (_view.y_to_note (ev->y));
	entered = true;
	queue_draw ();
	return true;
}

bool
PianoRollHeader::on_leave_notify_event (GdkEventCrossing*)
{
	if (!_scroomer_drag){
		get_window()->set_cursor();
	}
	invalidate_note_range(_highlighted_note, _highlighted_note);

	if (_clicked_note != NO_MIDI_NOTE) {
		reset_clicked_note (_clicked_note, _clicked_note != _highlighted_note);
	}

	_highlighted_note = NO_MIDI_NOTE;
	entered = false;
	queue_draw ();

	return true;
}

void
PianoRollHeader::note_range_changed ()
{
	_note_height = floor (_view.note_height ()) + 0.5f;
	queue_draw ();
}

void
PianoRollHeader::invalidate_note_range (int lowest, int highest)
{
	Glib::RefPtr<Gdk::Window> win = get_window();
	Gdk::Rectangle rect;

	lowest = max((int) _view.lowest_note(), lowest - 1);
	highest = min((int) _view.highest_note(), highest + 2);

	double y      = _view.note_to_y (highest);
	double height = _view.note_to_y (lowest - 1) - y;

	rect.set_x (0);
	rect.set_width (get_width ());
	rect.set_y ((int)floor (y));
	rect.set_height ((int)floor (height));

	if (win) {
		win->invalidate_rect (rect, false);
	}
	queue_draw ();
}

bool
PianoRollHeader::show_scroomer () const
{
	Editing::NoteNameDisplay nnd = UIConfiguration::instance().get_note_name_display();

	if (nnd == Editing::Never) {
		return false;
	}

	switch (editor().current_mouse_mode()) {
	case Editing::MouseDraw:
	case Editing::MouseContent:
		if (nnd == Editing::WithMIDNAM) {
			return have_note_names;
		} else {
			return true;
		}
	default:
		break;
	}
	return false;
}

void
PianoRollHeader::on_size_request (Gtk::Requisition* r)
{
	if (show_scroomer()) {
		_scroomer_size = 60.f * UIConfiguration::instance().get_ui_scale();
	} else {
		_scroomer_size = 20.f * UIConfiguration::instance().get_ui_scale();
	}

	r->width = _scroomer_size + 20.f;
}

void
PianoRollHeader::send_note_on (uint8_t note)
{
	std::shared_ptr<ARDOUR::MidiTrack> track = _view.trackview ().midi_track ();
	MidiTimeAxisView*                    mtv   = dynamic_cast<MidiTimeAxisView*> (&_view.trackview ());

	//cerr << "note on: " << (int) note << endl;

	if (track) {
		_event[0] = (MIDI_CMD_NOTE_ON | mtv->get_preferred_midi_channel ());
		_event[1] = note;
		_event[2] = 100;

		track->write_user_immediate_event (Evoral::MIDI_EVENT, 3, _event);
	}
}

void
PianoRollHeader::send_note_off (uint8_t note)
{
	std::shared_ptr<ARDOUR::MidiTrack> track = _view.trackview ().midi_track ();
	MidiTimeAxisView*                    mtv   = dynamic_cast<MidiTimeAxisView*> (&_view.trackview ());

	if (track) {
		_event[0] = (MIDI_CMD_NOTE_OFF | mtv->get_preferred_midi_channel ());
		_event[1] = note;
		_event[2] = 100;

		track->write_user_immediate_event (Evoral::MIDI_EVENT, 3, _event);
	}
}

void
PianoRollHeader::reset_clicked_note (uint8_t note, bool invalidate)
{
	_active_notes[note] = false;
	_clicked_note       = NO_MIDI_NOTE;
	send_note_off (note);
	if (invalidate) {
		invalidate_note_range (note, note);
	}
}

PublicEditor&
PianoRollHeader::editor () const
{
	return _view.trackview ().editor ();
}

void
PianoRollHeader::set_min_page_size(double page_size)
{
	_min_page_size = page_size;
};
