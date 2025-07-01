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

#include "ardour/instrument_info.h"
#include "ardour/midi_track.h"
#include "ardour/parameter_descriptor.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/rgb_macros.h"

#include "midi++/midnam_patch.h"

#include "editing.h"
#include "gui_thread.h"
#include "midi_view.h"
#include "midi_view_background.h"
#include "mouse_cursors.h"
#include "prh_base.h"
#include "editing_context.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtkmm2ext;

PianoRollHeaderBase::PianoRollHeaderBase (MidiViewBackground& bg)
	: _midi_context (bg)
	, _adj (_midi_context.note_range_adjustment)
	, _view (nullptr)
	, _font_descript (UIConfiguration::instance().get_NormalFont())
	, _font_descript_big_c (UIConfiguration::instance().get_NormalFont())
	, _font_descript_midnam (UIConfiguration::instance().get_NormalFont())
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
	for (int i = 0; i < 128; ++i) {
		_active_notes[i] = false;
	}

	_midi_context.NoteRangeChanged.connect (sigc::mem_fun (*this, &PianoRollHeaderBase::note_range_changed));
}

void
PianoRollHeaderBase::alloc_layouts (Glib::RefPtr<Pango::Context> context)
{
	_layout = Pango::Layout::create (context);
	_big_c_layout = Pango::Layout::create (context);
	_font_descript_big_c.set_absolute_size (10.0 * Pango::SCALE);
	_big_c_layout->set_font_description(_font_descript_big_c);
	_midnam_layout = Pango::Layout::create (context);
}

void
PianoRollHeaderBase::set_view (MidiView* v)
{
	_view = v;
	_midi_context.NoteRangeChanged.connect (sigc::mem_fun (*this, &PianoRollHeaderBase::note_range_changed));
}

bool
PianoRollHeaderBase::event_handler (GdkEvent* ev)
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

bool
PianoRollHeaderBase::scroll_handler (GdkEventScroll* ev)
{
	double evy = ev->y;
	double ignore;
	event_transform (ignore, evy);

	int note_range = _adj.get_page_size ();
	int note_lower = _adj.get_value ();

	if(ev->state == GDK_SHIFT_MASK){
		switch (ev->direction) {
		case GDK_SCROLL_UP: //ZOOM IN
			_midi_context.apply_note_range (min(note_lower + 1, 127), max(note_lower + note_range - 1,0), true);
			break;
		case GDK_SCROLL_DOWN: //ZOOM OUT
			_midi_context.apply_note_range (max(note_lower - 1,0), min(note_lower + note_range + 1, 127), true);
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

	set_note_highlight (_midi_context.y_to_note (event_y_to_y (evy)));

	_adj.value_changed ();
	redraw ();
	return true;
}

void
PianoRollHeaderBase::render (ArdourCanvas::Rect const & self, ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> cr) const
{
	int lowest;
	Gtkmm2ext::Color bg;

	double y2 = min (self.y1, (ArdourCanvas::Coord) _midi_context.contents_height());
	double context_note_height = _midi_context.note_height();

	//Reduce the frequency of Pango layout resizing
	//if (int(_old_context_note_height) != int(context_note_height)) {
	//Set Pango layout keyboard c's size
	_font_descript.set_absolute_size (context_note_height * 0.5 * Pango::SCALE);
	_layout->set_font_description(_font_descript);

	//change mode of midnam display
	if (context_note_height >= 8.0) {
		_mini_map_display = false;
	} else {
		_mini_map_display = true;
	}

	//Set Pango layout midnam size
	_font_descript_midnam.set_absolute_size (max(8.0 * 0.7 * Pango::SCALE, (int)context_note_height * 0.7 * Pango::SCALE));

	_midnam_layout->set_font_description(_font_descript_midnam);

	lowest = max(_midi_context.lowest_note(), _midi_context.y_to_note(y2));

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
	Gtkmm2ext::Color black           = UIConfiguration::instance().color (X_("piano key black"));
	Gtkmm2ext::Color white_highlight = UIConfiguration::instance().color (X_("piano key highlight"));
	Gtkmm2ext::Color black_highlight = UIConfiguration::instance().color (X_("piano key highlight"));
	Gtkmm2ext::Color textc           = UIConfiguration::instance().color (X_("gtk_foreground"));

	std::vector<int> numbers;;
	std::vector<int> positions;
	std::vector<int> heights;

	_midi_context.get_note_positions (numbers, positions, heights);

	/* Apply translation so we can use our natural coordinates to draw */

	double origin_x = 0.;
	double origin_y = 0.;
	draw_transform (origin_x, origin_y);

	cr->save ();

	/* XXX this is s horrible hack. For some reason, there is an off-by-one
	 * error with the drawing transform for the Gtk::Widget-derived version
	 * of this class. It is unclear right now (June 2025) where the extra
	 * pixel offset comes from. Correct it only for the case where the
	 * transform does nothing, which is (currently) only true for the
	 * Gtk::Widget case.
	 */

	if (origin_y == 0.) {
		origin_y -= 1;
	}


	cr->translate (origin_x, origin_y);

	// Render the MIDNAM text or its equivalent.  First, set up a clip
	// region so that the text doesn't spill, regardless of its length.

	cr->save ();
	cr->rectangle (0,0,_scroomer_size, height ());
	cr->clip();

	if (show_scroomer()) {

		/* Draw the actual text */

		for (std::vector<int>::size_type n = 0; n < numbers.size(); ++n) {

			int size_x, size_y;
			int y = positions[n];
			NoteName const & note (note_names[numbers[n]]);

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
					set_source_rgba (cr, textc);
				}
				pango_layout_get_pixel_size (_midnam_layout->gobj (), &size_x, &size_y);
				cr->rectangle (2.f, y + (context_note_height * 0.5), size_x, context_note_height * 0.2);
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
		cr->rectangle (_scroomer_size - fade_width, 0, fade_width, height ());
		cr->fill();
	}

	/* Now draw the semi-transparent scroomer over the top */

	render_scroomer (cr);

	/* Done with clip region */

	cr->restore();

	/* Setup a cairo translation so that all drawing can be done using item
	 * coordinate
	 */

	/* Now draw black/white rects for each note, following standard piano
	   layout, but without a setback/offset for the black keys
	*/

	for (std::vector<int>::size_type n = 0; n < numbers.size(); ++n) {

		int i = numbers[n];
		int oct_rel = i % 12;

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
			break;

		case 0:
		case 2:
		case 4:
		case 5:
		case 7:
		case 9:
		case 11:
			/* white note */
			if (i == _highlighted_note) {
				bg = white_highlight;
			} else {
				bg  = white;
			}
			break;
		default:
			break;

		}


		Gtkmm2ext::set_source_rgba (cr, bg);

		double x = _scroomer_size;;
		double y = positions[n];

		cr->rectangle (x, y, width() - 1., heights[n]);
		cr->fill ();

		if ((oct_rel == 4 || oct_rel == 11) && y > 0) {
			/* draw black separators between B/C and E/F */
			Gtkmm2ext::set_source_rgba (cr, black);
			cr->set_line_width (1.0);
			/* move the divider up to match the rect-drawing
			   semantics used for MidiViewBackground's note lines,
			   which are rects
			*/
			cr->move_to (x, y + 0.5);
			cr->line_to (x + width(), y + 0.5);
			cr->stroke ();
		}
	}

	/* render the C<N> of the key, when key is too small to contain text we
	   place the C<N> on the midnam scroomer area.

	   we render an additional 5 notes below the lowest note displayed
	   so that the top of the C is shown to maintain visual context
	 */

	int bc_height, c_height, ignore;

	_big_c_layout->set_text ("C1");
	_layout->set_text ("C1");

	pango_layout_get_pixel_size (_big_c_layout->gobj(), &ignore, &bc_height);
	pango_layout_get_pixel_size (_layout->gobj(), &ignore, &c_height);

	for (std::vector<int>::size_type n = 0; n < numbers.size(); ++n) {

		double h = heights[n];
		double x = _scroomer_size;
		double y = positions[n];
		int oct_rel = numbers[n] % 12;

		if (oct_rel == 0 || (oct_rel == 7 && _adj.get_page_size() <=10)) {

			std::stringstream str;
			int cn = numbers[n] / 12 - 1;

			if (oct_rel == 0){
				str << 'C' << cn;
			} else {
				str << 'G' << cn;
			}

			if (h > 12.){
				/* Cn text shown in keys */
				set_source_rgba (cr, black);
				_layout->set_text (str.str());
				cr->move_to (x, y);
				_layout->show_in_cairo_context (cr);
			} else {
				/* Cn text shown to left of keys */
				set_source_rgba (cr, textc);
				_big_c_layout->set_text (str.str());
				/* XXX magic number alert - negative offset to
				 * get left of the keys
				 */
				cr->move_to (x - 18, y);
				_big_c_layout->show_in_cairo_context (cr);
			}
		}
	}

	cr->restore ();
}

void
PianoRollHeaderBase::render_scroomer (Cairo::RefPtr<Cairo::Context> cr) const
{
	double scroomer_top = max (1.0, (1.0 - ((_adj.get_value()+_adj.get_page_size()) / 127.0)) * height());
	double scroomer_bottom = (1.0 - (_adj.get_value () / 127.0)) * height ();

	Gtkmm2ext::Color c = UIConfiguration::instance().color_mod (X_("scroomer"), X_("scroomer alpha"));
	Gtkmm2ext::Color save_color (c);

	if (entered) {
		c = HSV (c).lighter (0.25).color();
	}

	double x = 0.;
	double y = 0.;
	draw_transform (x, y);

	cr->save ();
	cr->translate (x, y);

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

	cr->restore ();
}

void
PianoRollHeaderBase::instrument_info_change ()
{
	have_note_names = false;

	for (int i = 0; i < 128; ++i) {
		note_names[i] = get_note_name (i);

		if (note_names[i].from_midnam) {
			have_note_names = true;
		}
	}

	_queue_resize ();
}

PianoRollHeaderBase::NoteName
PianoRollHeaderBase::get_note_name (int note)
{
	using namespace MIDI::Name;
	std::string name;
	NoteName rtn;

	ARDOUR::InstrumentInfo* ii = _midi_context.instrument_info();

	if (!ii) {
		return rtn;
	}

	int midnam_channel = _midi_context.get_preferred_midi_channel ();

	name = ii->get_note_name (
		0,               //bank
		0,               //program
		midnam_channel,  //channel
		note);           //note

	rtn.name = name.empty() ? ARDOUR::ParameterDescriptor::midi_note_name (note) : name;
	rtn.from_midnam = !name.empty();
	return rtn;
}

bool
PianoRollHeaderBase::motion_handler (GdkEventMotion* ev)
{
	/* event coordinates are in canvas/window space */

	double evy = ev->y;
	double ignore;
	event_transform (ignore, evy);

	if (!_scroomer_drag && ev->x < _scroomer_size){

		double scroomer_top = max (1.0, (1.0 - ((_adj.get_value()+_adj.get_page_size()) / 127.0)) * height());
		double scroomer_bottom = (1.0 - (_adj.get_value () / 127.0)) * height();
		double edge = 5. * UIConfiguration::instance().get_ui_scale();

		if (evy > scroomer_top - edge && evy < scroomer_top + edge){
			if (_scroomer_state != TOP) {
				set_cursor (_midi_context.editing_context().cursors()->resize_top);
				_scroomer_state = TOP;
			}
		} else if (evy > scroomer_bottom - edge && evy < scroomer_bottom + edge){
			if (_scroomer_state != BOTTOM) {
				set_cursor (_midi_context.editing_context().cursors()->resize_bottom);
				_scroomer_state = BOTTOM;
			}
		} else {
			if (_scroomer_state != MOVE) {
				set_cursor (_midi_context.editing_context().cursors()->grabber);
				_scroomer_state = MOVE;
			}
		}
	}

	if (_scroomer_drag){

		const double pixels_per_note = 127.0 / height();
		double delta = _old_y - evy;
		double val_at_pointer = (delta * pixels_per_note);
		double real_val_at_pointer = 127.0 - (evy * pixels_per_note);
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
			real_val_at_pointer = min (127.0, ceil (real_val_at_pointer));

			if (_midi_context.note_height() >= UIConfiguration::instance().get_max_note_height()){
				_saved_top_val  = min (_adj.get_value() + _adj.get_page_size (), 127.0);
			} else {
				_saved_top_val = 0.0;
				// _midi_context.apply_note_range (_adj.get_value (), real_val_at_pointer, true, MidiViewBackground::CanMoveTop);
				idle_lower = _adj.get_value();
				idle_upper = real_val_at_pointer;
				if (!scroomer_drag_connection.connected()) {
					scroomer_drag_connection = Glib::signal_idle().connect (sigc::mem_fun (*this, &PianoRollHeaderBase::idle_apply_range));
				}
			}
			break;

		case BOTTOM:
			real_val_at_pointer = real_val_at_pointer >= _saved_bottom_val? _adj.get_value() : real_val_at_pointer;
			real_val_at_pointer = max (0.0, floor (real_val_at_pointer));
			if (_midi_context.note_height() >= UIConfiguration::instance().get_max_note_height()){
				_saved_bottom_val  = _adj.get_value();
			} else {
				_saved_bottom_val = 127.0;
				// _midi_context.apply_note_range (real_val_at_pointer, _adj.get_value () + _adj.get_page_size (), true, MidiViewBackground::CanMoveBottom);
				idle_lower = real_val_at_pointer;
				idle_upper = _adj.get_value();
				if (!scroomer_drag_connection.connected()) {
					scroomer_drag_connection = Glib::signal_idle().connect (sigc::mem_fun (*this, &PianoRollHeaderBase::idle_apply_range));
				}
			}
			break;

		default:
			break;
		}

		redraw ();

	} else {

		int note = _midi_context.y_to_note(evy);
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

	_old_y = evy;

	return true;
}

bool
PianoRollHeaderBase::idle_apply_range ()
{
	_midi_context.apply_note_range (idle_lower, idle_upper, true, MidiViewBackground::CanMoveBottom);
	scroomer_drag_connection.disconnect ();
	return false;
}

void
PianoRollHeaderBase::begin_scroomer_drag (double evy)
{
	_scroomer_drag = true;
	_old_y = evy;
	_fract = _adj.get_value();
	_fract_top = _adj.get_value() + _adj.get_page_size();
}

void
PianoRollHeaderBase::end_scroomer_drag ()
{
	_scroomer_drag = false;
}

bool
PianoRollHeaderBase::button_press_handler (GdkEventButton* ev)
{
	double evy = ev->y;
	double ignore;
	event_transform (ignore, evy);

	/* Convert canvas-coordinates to item coordinates */

	_scroomer_button_state = _scroomer_state;


	if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {
		if (_midi_context.visibility_range_style () == MidiViewBackground::FullRange) {
			_midi_context.set_note_visibility_range_style (MidiViewBackground::ContentsRange);
		} else {
			_midi_context.set_note_visibility_range_style (MidiViewBackground::FullRange);
		}
		return true;
	}

	if (ev->x <= _scroomer_size) {

		/* button press on scroomer handler */

		if (ev->button != 1) {
			return true;
		}

		begin_scroomer_drag (evy);
		return true;

	} else {

		/* button press on note keys */

		int note = _midi_context.y_to_note (evy);

		bool tertiary = Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier);
		bool primary = Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier);
		bool is_selection_click = (ev->type == GDK_BUTTON_PRESS) && (ev->button < 3) && Keyboard::no_modifier_keys_pressed (ev);

		/* Note that shift-button1 actually ends up invoking
		 * ExtendNoteSelection, but this has the same effect as
		 * SetNoteSelection when there is no existing selection
		 */

		if (is_selection_click) {
			SetNoteSelection (note); // EMIT SIGNAL
		} else if (tertiary && (ev->button == 1 || ev->button == 2)) {
			ExtendNoteSelection (note); // EMIT SIGNAL
		} else if (primary && (ev->button == 1 || ev->button == 2)) {
			ToggleNoteSelection (note); // EMIT SIGNAL
		}

		if (ev->type == GDK_BUTTON_PRESS && ev->button == 1 && note >= 0 && note < 128) {
			do_grab ();
			_dragging = true;

			if (!_active_notes[note]) {
				_active_notes[note] = true;
				_clicked_note = note;
				send_note_on (note);

				invalidate_note_range (note, note);
			} else {
				reset_clicked_note (note);
			}
		}
	}

	return true;
}

bool
PianoRollHeaderBase::button_release_handler (GdkEventButton* ev)
{
	double evy = ev->y;
	double ignore;
	event_transform (ignore, evy);

	end_scroomer_drag ();

	int note = _midi_context.y_to_note (evy);

	if (_dragging) {

		do_ungrab ();
		reset_clicked_note (_clicked_note);
		_dragging = false;
	}
	return true;
}

void
PianoRollHeaderBase::set_note_highlight (uint8_t note)
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
PianoRollHeaderBase::enter_handler (GdkEventCrossing* ev)
{
	double evy = ev->y;
	double ignore;
	event_transform (ignore, evy);

	set_note_highlight (_midi_context.y_to_note (evy));
	set_cursor (_midi_context.editing_context().cursors()->selector);
	entered = true;
	redraw ();
	return true;
}

bool
PianoRollHeaderBase::leave_handler (GdkEventCrossing*)
{
	set_cursor (nullptr);

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
PianoRollHeaderBase::note_range_changed ()
{
	redraw ();
}

void
PianoRollHeaderBase::invalidate_note_range (int lowest, int highest)
{
	lowest = max ((int) _midi_context.lowest_note(), lowest - 1);
	highest = min ((int) _midi_context.highest_note(), highest + 2);

	double y = _midi_context.note_to_y (highest);
	double h = _midi_context.note_to_y (lowest - 1) - y;

	redraw (0, y, width(), h);
}

bool
PianoRollHeaderBase::show_scroomer () const
{
	Editing::NoteNameDisplay nnd = UIConfiguration::instance().get_note_name_display();

	if (nnd == Editing::Never) {
		return false;
	}

	switch (_midi_context.editing_context().current_mouse_mode()) {
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
PianoRollHeaderBase::send_note_on (uint8_t note)
{
	std::shared_ptr<ARDOUR::MidiTrack> track = midi_track ();

	if (track) {
		_event[0] = (MIDI_CMD_NOTE_ON | _midi_context.get_preferred_midi_channel ());
		_event[1] = note;
		_event[2] = 100;

		track->write_user_immediate_event (Evoral::MIDI_EVENT, 3, _event);
	}
}

void
PianoRollHeaderBase::send_note_off (uint8_t note)
{
	std::shared_ptr<ARDOUR::MidiTrack> track = midi_track ();

	if (track) {
		_event[0] = (MIDI_CMD_NOTE_OFF | _midi_context.get_preferred_midi_channel ());
		_event[1] = note;
		_event[2] = 100;

		track->write_user_immediate_event (Evoral::MIDI_EVENT, 3, _event);
	}
}

void
PianoRollHeaderBase::reset_clicked_note (uint8_t note, bool invalidate)
{
	_active_notes[note] = false;
	_clicked_note       = NO_MIDI_NOTE;
	send_note_off (note);
	if (invalidate) {
		invalidate_note_range (note, note);
	}
}

void
PianoRollHeaderBase::set_min_page_size(double page_size)
{
	_min_page_size = page_size;
}

void
PianoRollHeaderBase::set_cursor (Gdk::Cursor* cursor)
{
	Glib::RefPtr<Gdk::Window> win = cursor_window ();

	if (win && !_midi_context.editing_context().cursors()->is_invalid (cursor)) {
		gdk_window_set_cursor (win->gobj(), cursor ? cursor->gobj() : nullptr);
		gdk_flush ();
	}
}
