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

#include <iostream>

#include "evoral/midi_events.h"

#include "canvas/canvas.h"

#include "ardour/midi_track.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"

#include "midi++/midnam_patch.h"

#include "editing.h"
#include "gui_thread.h"
#include "midi_view.h"
#include "midi_view_background.h"
#include "mouse_cursors.h"
#include "prh.h"
#include "editing_context.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtkmm2ext;

namespace ArdourCanvas {

PianoRollHeader::PianoRollHeader (Item* parent, MidiViewBackground& bg)
	: Rectangle (parent)
	, _midi_context (bg)
	, _adj (_midi_context.note_range_adjustment)
	, _view (nullptr)
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
	, have_note_names (false)
{
	Glib::RefPtr<Pango::Context> context = _canvas->get_pango_context();

	_layout = Pango::Layout::create (context);
	_big_c_layout = Pango::Layout::create (context);
	_font_descript_big_c.set_absolute_size (10.0 * Pango::SCALE);
	_big_c_layout->set_font_description(_font_descript_big_c);
	_midnam_layout = Pango::Layout::create (context);

	for (int i = 0; i < 128; ++i) {
		_active_notes[i] = false;
	}

	resize ();
	bg.HeightChanged.connect (height_connection, MISSING_INVALIDATOR, std::bind (&PianoRollHeader::resize, this), gui_context());

	/* draw vertical lines on both sides of the rectangle */
	set_fill (false);
	set_fill (true);
	set_outline_color (0x000000ff); /* XXX theme me */
	set_outline_what (Rectangle::What (Rectangle::LEFT|Rectangle::RIGHT));

	Event.connect (sigc::mem_fun (*this, &PianoRollHeader::event_handler));
}

void
PianoRollHeader::resize ()
{
	double w, h;
	size_request (w, h);
	set (Rect (0., 0., w, h));
}

void
PianoRollHeader::set_view (MidiView* v)
{
	_view = v;
	if (_view) {
		_view->midi_context().NoteRangeChanged.connect (sigc::mem_fun (*this, &PianoRollHeader::note_range_changed));
	}
}

void
PianoRollHeader::size_request (double& w, double& h) const
{
	h = _midi_context.contents_height();

	if (show_scroomer()) {
		_scroomer_size = 60.f * UIConfiguration::instance().get_ui_scale();
	} else {
		_scroomer_size = 20.f * UIConfiguration::instance().get_ui_scale();
	}

	w = _scroomer_size +  20.;
}

bool
PianoRollHeader::event_handler (GdkEvent* ev)
{
	/* Remember that ev uses canvas coordinates, not item */

	switch (ev->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		return button_press_handler (&ev->button);

	case GDK_BUTTON_RELEASE:
		return button_release_handler (&ev->button);

	case GDK_ENTER_NOTIFY:
		return enter_handler (&ev->crossing);

	case GDK_LEAVE_NOTIFY:
		return leave_handler (&ev->crossing);

	case GDK_SCROLL:
		return scroll_handler (&ev->scroll);

	case GDK_MOTION_NOTIFY:
		return motion_handler (&ev->motion);

	default:
		break;
	}

	return false;
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
render_rect(Cairo::RefPtr<Cairo::Context> cr, int note, double x[], double y[], Gtkmm2ext::Color& bg)
{
	set_source_rgba (cr, bg);
	create_path (cr, x, y, 0, 4);
	cr->fill ();
}

void
PianoRollHeader::render_scroomer (Cairo::RefPtr<Cairo::Context> cr) const
{
	double scroomer_top = max (1.0, (1.0 - ((_adj.get_value()+_adj.get_page_size()) / 127.0)) * get().height () );
	double scroomer_bottom = (1.0 - (_adj.get_value () / 127.0)) * get().height ();

	Gtkmm2ext::Color c = UIConfiguration::instance().color_mod (X_("scroomer"), X_("scroomer alpha"));
	Gtkmm2ext::Color save_color (c);

	if (entered) {
		c = HSV (c).lighter (0.25).color();
	}

	set_source_rgba (cr, c);
	cr->move_to (1.f, scroomer_top);
	cr->line_to (_scroomer_size - 1.f, scroomer_top);
	cr->line_to (_scroomer_size - 1.f, scroomer_bottom);
	cr->line_to (1.f, scroomer_bottom);
	cr->line_to (1.f, scroomer_top);
	cr->fill();

	if (entered) {
		cr->save ();
		c = HSV (save_color).lighter (0.9).color();
		set_source_rgba (cr, c);
		cr->set_line_width (4.);
		cr->move_to (1.f, scroomer_top + 2.);
		cr->line_to (_scroomer_size - 1.f, scroomer_top + 2.);
		cr->stroke ();
		cr->line_to (_scroomer_size - 1.f, scroomer_bottom - 2.);
		cr->line_to (2.f, scroomer_bottom - 2.);
		cr->stroke ();
		cr->restore ();
	}
}

bool
PianoRollHeader::scroll_handler (GdkEventScroll* ev)
{
	if (!_view) {
		return false;
	}

	int note_range = _adj.get_page_size ();
	int note_lower = _adj.get_value ();

	if(ev->state == GDK_SHIFT_MASK){
		switch (ev->direction) {
		case GDK_SCROLL_UP: //ZOOM IN
			_view->apply_note_range (min(note_lower + 1, 127), max(note_lower + note_range - 1,0), true);
			break;
		case GDK_SCROLL_DOWN: //ZOOM OUT
			_view->apply_note_range (max(note_lower - 1,0), min(note_lower + note_range + 1, 127), true);
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

	Duple evd (canvas_to_item (Duple (ev->x, ev->y)));
	set_note_highlight (_view->midi_context().y_to_note (evd.y));

	_adj.value_changed ();
	redraw ();
	return true;
}


void
PianoRollHeader::get_path (int note, double x[], double y[]) const
{
	double y_pos = floor(_midi_context.note_to_y(note));
	double note_height;
	double width = get().width() - 1.0f;

	if (note == 0) {
		note_height = floor(_midi_context.contents_height()) - y_pos;
	} else {
		note_height = _midi_context.note_height() <= 3 ? _midi_context.note_height() : _midi_context.note_height() - 1.f;
	}

	x[0] = _scroomer_size;
	y[0] = y_pos + note_height;

	x[1] = _scroomer_size;
	y[1] = y_pos;

	x[2] = width;
	y[2] = y_pos;

	x[3] = width;
	y[3] = y_pos + note_height;

	x[4] = x[0];
	y[4] = y[0];
}

void
PianoRollHeader::render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> cr) const
{
	int lowest, highest;
	Gtkmm2ext::Color bg;
	double x[9];
	double y[9];
	int oct_rel;

	Rectangle::render (area, cr);

	/* Setup a cairo translation so that all drawing can be done using item
	 * coordinate
	 */

	Duple origin (item_to_window (Duple (0., 0.)));

	cr->save ();
	cr->translate (origin.x, origin.y);

	Rect self (get());

	double y1 = max (self.y0, 0.);
	double y2 = min (self.y1, (ArdourCanvas::Coord) floor(_midi_context.contents_height()));
	double av_note_height = _midi_context.note_height();
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

	lowest = max(_midi_context.lowest_note(), _midi_context.y_to_note(y2));
	highest = min(_midi_context.highest_note(), _midi_context.y_to_note(y1));

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


	// Render the MIDNAM text or its equivalent.  First, set up a clip
	// region so that the text doesn't spill, regardless of its length.

	cr->save();

	cr->rectangle (0,0,_scroomer_size, get().height () );
	cr->clip();

	if (show_scroomer()) {

		/* Draw the actual text */

		for (int i = lowest; i <= highest; ++i) {
			int size_x, size_y;
			double y = floor(_midi_context.note_to_y(i)) - 0.5f;
			NoteName const & note (note_names[i]);

			_midnam_layout->set_text (note.name);

			set_source_rgba (cr, textc);
			cr->move_to (2.f, y);

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
		cr->rectangle (_scroomer_size - fade_width, 0, _scroomer_size, get().height () );
		cr->fill();
	}

	/* Now draw the semi-transparent scroomer over the top */

	render_scroomer (cr);

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
		double y = floor(_midi_context.note_to_y(i)) - 0.5f;
		double note_height = i == 0? av_note_height : floor(_midi_context.note_to_y(i - 1)) - y;
		oct_rel = i % 12;

		if (oct_rel == 0 || (oct_rel == 7 && _adj.get_page_size() <=10)) {
			std::stringstream s;

			int cn = i / 12 - 1;

			if (oct_rel == 0){
				s << 'C' << cn;
			} else {
				s << 'G' << cn;
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

	/* Done with translation for item->window */
	cr->restore ();
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

	if (_view) {
		_view->midi_track()->gui_changed ("visible_tracks", (void *) 0); /* EMIT SIGNAL */
	}

}

PianoRollHeader::NoteName
PianoRollHeader::get_note_name (int note)
{
	using namespace MIDI::Name;
	std::string name;
	std::string note_n;
	NoteName rtn;

#if 0
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
	switch (oct_rel) {
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

#endif
	std::string new_string = std::string(3 - std::to_string(note).length(), '0') + std::to_string(note);
	rtn.name = name.empty()? new_string + " " + note_n : name;
	rtn.from_midnam = !name.empty();
	return rtn;
}

bool
PianoRollHeader::motion_handler (GdkEventMotion* ev)
{
	if (!_view) {
		return false;
	}

	Duple evd (canvas_to_item (Duple (ev->x, ev->y)));

	if (!_scroomer_drag && ev->x < _scroomer_size){

		double scroomer_top = max (1.0, (1.0 - ((_adj.get_value()+_adj.get_page_size()) / 127.0)) * get().height());
		double scroomer_bottom = (1.0 - (_adj.get_value () / 127.0)) * get().height();
		double edge = 5. * UIConfiguration::instance().get_ui_scale();

		if (evd.y > scroomer_top - 5 && evd.y < scroomer_top + edge){
			if (_scroomer_state != TOP) {
				_view->editing_context().set_canvas_cursor (_view->editing_context().cursors()->resize_top);
				_scroomer_state = TOP;
			}
		} else if (evd.y > scroomer_bottom - edge && evd.y < scroomer_bottom + edge){
			if (_scroomer_state != BOTTOM) {
				_view->editing_context().set_canvas_cursor (_view->editing_context().cursors()->resize_bottom);
				_scroomer_state = BOTTOM;
			}
		} else {
			if (_scroomer_state != MOVE) {
				_view->editing_context().set_canvas_cursor (_view->editing_context().cursors()->grabber);
				_scroomer_state = MOVE;
			}
		}
	}

	if (_scroomer_drag){

		double pixel2val = 127.0 / get().height();
		double delta = _old_y - evd.y;
		double val_at_pointer = (delta * pixel2val);
		double real_val_at_pointer = 127.0 - (evd.y * pixel2val);
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
				if (_midi_context.note_height() >= UIConfiguration::instance().get_max_note_height()){
					_saved_top_val  = min(_adj.get_value() + _adj.get_page_size (), 127.0);
				} else {
					_saved_top_val = 0.0;
				}
				//if we are at largest note size & the user is moving down don't do anything
				//FIXME we are using a heuristic of 18.5 for max note size, but this changes when track size is small to 19.5?
				_midi_context.apply_note_range (_adj.get_value (), real_val_at_pointer, true);
				break;
			case BOTTOM:
				real_val_at_pointer = max(0.0, real_val_at_pointer);
				real_val_at_pointer = real_val_at_pointer >= _saved_bottom_val? _adj.get_value() : real_val_at_pointer;
				if (_midi_context.note_height() >= UIConfiguration::instance().get_max_note_height()){
					_saved_bottom_val  = _adj.get_value();
				} else {
					_saved_bottom_val = 127.0;
				}
				_midi_context.apply_note_range (real_val_at_pointer, _adj.get_value () + _adj.get_page_size (), true);
				break;
			default:
				break;
		}

	} else {

		int note = _midi_context.y_to_note(evd.y);
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

	redraw ();
	_old_y = evd.y;
	//win->process_updates(false);

	return true;
}

bool
PianoRollHeader::button_press_handler (GdkEventButton* ev)
{
	if (!_view) {
		return false;
	}

	/* Convert canvas-coordinates to item coordinates */
	Duple evd (canvas_to_item (Duple (ev->x, ev->y)));

	_scroomer_button_state = _scroomer_state;

	if (ev->button == 1 && ev->x <= _scroomer_size){

		if (ev->type == GDK_2BUTTON_PRESS) {
			_view->set_visibility_note_range (MidiStreamView::ContentsRange, false);
			return true;
		}

		_scroomer_drag = true;
		_old_y = evd.y;
		_fract = _adj.get_value();
		_fract_top = _adj.get_value() + _adj.get_page_size();
		return true;

	} else {
		int note = _midi_context.y_to_note(evd.y);
		bool tertiary = Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier);
		bool primary = Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier);

		if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {
			if (primary) {
				_adj.set_value (0.0);
				_adj.set_page_size (127.0);
				_adj.value_changed ();
				redraw ();
				return false;
			}
			return false;
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
			grab ();
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
PianoRollHeader::button_release_handler (GdkEventButton* ev)
{
	Duple evd (canvas_to_item (Duple (ev->x, ev->y)));

	_scroomer_drag = false;

	int note = _midi_context.y_to_note(evd.y);

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
			ungrab ();

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
PianoRollHeader::enter_handler (GdkEventCrossing* ev)
{
	Duple evd (canvas_to_item (Duple (ev->x, ev->y)));
	set_note_highlight (_midi_context.y_to_note (evd.y));
	entered = true;
	redraw ();
	return true;
}

bool
PianoRollHeader::leave_handler (GdkEventCrossing*)
{
	if (!_scroomer_drag){
		if (_view) {
			/* XXX we used to pop the cursor stack here */
		}
	}
	invalidate_note_range(_highlighted_note, _highlighted_note);

	if (_clicked_note != NO_MIDI_NOTE) {
		reset_clicked_note (_clicked_note, _clicked_note != _highlighted_note);
	}

	_highlighted_note = NO_MIDI_NOTE;
	entered = false;
	redraw ();

	return true;
}

void
PianoRollHeader::note_range_changed ()
{
	redraw ();
}

void
PianoRollHeader::invalidate_note_range (int lowest, int highest)
{
	lowest = max((int) _midi_context.lowest_note(), lowest - 1);
	highest = min((int) _midi_context.highest_note(), highest + 2);

	double y      = _midi_context.note_to_y (highest);
	double height = _midi_context.note_to_y (lowest - 1) - y;

	dynamic_cast<ArdourCanvas::GtkCanvas*>(_canvas)->queue_draw_area (0., floor (y), get().width(), floor (height));
}

bool
PianoRollHeader::show_scroomer () const
{
	if (!_view) {
		return false;
	}

	Editing::NoteNameDisplay nnd = UIConfiguration::instance().get_note_name_display();

	if (nnd == Editing::Never) {
		return false;
	}

	switch (_view->editing_context().current_mouse_mode()) {
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
PianoRollHeader::send_note_on (uint8_t note)
{
	if (!_view) {
		return;
	}

	std::shared_ptr<ARDOUR::MidiTrack> track = _view->midi_track ();

	//cerr << "note on: " << (int) note << endl;

	if (track) {
		_event[0] = (MIDI_CMD_NOTE_ON | _midi_context.get_preferred_midi_channel ());
		_event[1] = note;
		_event[2] = 100;

		track->write_user_immediate_event (Evoral::MIDI_EVENT, 3, _event);
	}
}

void
PianoRollHeader::send_note_off (uint8_t note)
{
	if (!_view) {
		return;
	}

	std::shared_ptr<ARDOUR::MidiTrack> track = _view->midi_track ();

	if (track) {
		_event[0] = (MIDI_CMD_NOTE_OFF | _midi_context.get_preferred_midi_channel ());
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

void
PianoRollHeader::set_min_page_size(double page_size)
{
	_min_page_size = page_size;
};

}
