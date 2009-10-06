/*
    Copyright (C) 2001-2007 Paul Davis
    Author: Dave Robillard

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

#include <cmath>
#include <cassert>
#include <algorithm>
#include <ostream>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include <sigc++/signal.h>

#include "pbd/memento_command.h"

#include "ardour/playlist.h"
#include "ardour/tempo.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_model.h"
#include "ardour/midi_patch_manager.h"

#include "evoral/Parameter.hpp"
#include "evoral/Control.hpp"

#include "automation_region_view.h"
#include "automation_time_axis.h"
#include "canvas-hit.h"
#include "canvas-note.h"
#include "canvas-program-change.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_cut_buffer.h"
#include "midi_list_editor.h"
#include "midi_region_view.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "midi_time_axis.h"
#include "midi_util.h"
#include "public_editor.h"
#include "selection.h"
#include "simpleline.h"
#include "streamview.h"
#include "utils.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

MidiRegionView::MidiRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv,
		boost::shared_ptr<MidiRegion> r, double spu, Gdk::Color const & basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, _force_channel(-1)
	, _last_channel_selection(0xFFFF)
	, _default_note_length(1.0)
	, _current_range_min(0)
	, _current_range_max(0)
	, _model_name(string())
	, _custom_device_mode(string())
	, _active_notes(0)
	, _note_group(new ArdourCanvas::Group(*parent))
	, _delta_command(0)
	, _diff_command(0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
{
	_note_group->raise_to_top();
}

MidiRegionView::MidiRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv,
		boost::shared_ptr<MidiRegion> r, double spu, Gdk::Color& basic_color,
		TimeAxisViewItem::Visibility visibility)
	: RegionView (parent, tv, r, spu, basic_color, false, visibility)
	, _force_channel(-1)
	, _last_channel_selection(0xFFFF)
	, _default_note_length(1.0)
	, _model_name(string())
	, _custom_device_mode(string())
	, _active_notes(0)
	, _note_group(new ArdourCanvas::Group(*parent))
	, _delta_command(0)
	, _diff_command(0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	
{
	_note_group->raise_to_top();
}


MidiRegionView::MidiRegionView (const MidiRegionView& other)
	: sigc::trackable(other)
	, RegionView (other)
	, _force_channel(-1)
	, _last_channel_selection(0xFFFF)
	, _default_note_length(1.0)
	, _model_name(string())
	, _custom_device_mode(string())
	, _active_notes(0)
	, _note_group(new ArdourCanvas::Group(*get_canvas_group()))
	, _delta_command(0)
	, _diff_command(0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
{
	Gdk::Color c;
	int r,g,b,a;

	UINT_TO_RGBA (other.fill_color, &r, &g, &b, &a);
	c.set_rgb_p (r/255.0, g/255.0, b/255.0);
	
	init (c, false);
}

MidiRegionView::MidiRegionView (const MidiRegionView& other, boost::shared_ptr<MidiRegion> region)
	: RegionView (other, boost::shared_ptr<Region> (region))
	, _force_channel(-1)
	, _last_channel_selection(0xFFFF)
	, _default_note_length(1.0)
	, _model_name(string())
	, _custom_device_mode(string())
	, _active_notes(0)
	, _note_group(new ArdourCanvas::Group(*get_canvas_group()))
	, _delta_command(0)
	, _diff_command(0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
{
	Gdk::Color c;
	int r,g,b,a;

	UINT_TO_RGBA (other.fill_color, &r, &g, &b, &a);
	c.set_rgb_p (r/255.0, g/255.0, b/255.0);

	init (c, true);
}

void
MidiRegionView::init (Gdk::Color const & basic_color, bool wfd)
{
	if (wfd) {
		midi_region()->midi_source(0)->load_model();
	}

	_model = midi_region()->midi_source(0)->model();
	_enable_display = false;

	RegionView::init (basic_color, false);

	compute_colors (basic_color);

	set_height (trackview.current_height());

	region_muted ();
	region_sync_changed ();
	region_resized (BoundsChanged);
	region_locked ();
	
	reset_width_dependent_items (_pixel_width);

	set_colors ();

	_enable_display = true;
	if (_model) {
		if (wfd) {
			display_model (_model);
		}
	}

	group->raise_to_top();
	group->signal_event().connect (mem_fun (this, &MidiRegionView::canvas_event), false);

	midi_view()->signal_channel_mode_changed().connect(
			mem_fun(this, &MidiRegionView::midi_channel_mode_changed));
	
	midi_view()->signal_midi_patch_settings_changed().connect(
			mem_fun(this, &MidiRegionView::midi_patch_settings_changed));
}

bool
MidiRegionView::canvas_event(GdkEvent* ev)
{
	PublicEditor& editor (trackview.editor());

	if (!editor.internal_editing()) {
		return false;
	}

	static double drag_start_x, drag_start_y;
	static double last_x, last_y;
	double event_x, event_y;
	nframes64_t event_frame = 0;
	bool fine;

	static ArdourCanvas::SimpleRect* drag_rect = 0;

	/* XXX: note that as of August 2009, the GnomeCanvas does not propagate scroll events
	   to its items, which means that ev->type == GDK_SCROLL will never be seen
	*/

	switch (ev->type) {
	case GDK_SCROLL:
		fine = Keyboard::modifier_state_equals (ev->scroll.state, Keyboard::Level4Modifier);
		
		if (ev->scroll.direction == GDK_SCROLL_UP) {
			change_velocities (true, fine, false);
			return true;
		} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
			change_velocities (false, fine, false);
			return true;
		} else {
			return false;
		}
		break;

	case GDK_KEY_PRESS:

		/* since GTK bindings are generally activated on press, and since
		   detectable auto-repeat is the name of the game and only sends
		   repeated presses, carry out key actions at key press, not release.
		*/

		if (ev->key.keyval == GDK_Alt_L || ev->key.keyval == GDK_Alt_R){
			_mouse_state = SelectTouchDragging;
			return true;

		} else if (ev->key.keyval == GDK_Escape) {
			clear_selection();
			_mouse_state = None;

		} else if (ev->key.keyval == GDK_comma || ev->key.keyval == GDK_period) {

			bool start = (ev->key.keyval == GDK_comma);
			bool end = (ev->key.keyval == GDK_period);
			bool shorter = Keyboard::modifier_state_contains (ev->key.state, Keyboard::PrimaryModifier);
			fine = Keyboard::modifier_state_contains (ev->key.state, Keyboard::SecondaryModifier);
			
			change_note_lengths (fine, shorter, start, end);

			return true;

		} else if (ev->key.keyval == GDK_Delete) {

			delete_selection();
			return true;

		} else if (ev->key.keyval == GDK_Tab) {

			if (Keyboard::modifier_state_equals (ev->key.state, Keyboard::PrimaryModifier)) {
				goto_previous_note ();
			} else {
				goto_next_note ();
			}
			return true;

		} else if (ev->key.keyval == GDK_Up) {

			bool allow_smush = Keyboard::modifier_state_contains (ev->key.state, Keyboard::SecondaryModifier);
			bool fine = Keyboard::modifier_state_contains (ev->key.state, Keyboard::TertiaryModifier);

			if (Keyboard::modifier_state_contains (ev->key.state, Keyboard::PrimaryModifier)) {
				change_velocities (true, fine, allow_smush);
			} else {
				transpose (true, fine, allow_smush);
			}
			return true;

		} else if (ev->key.keyval == GDK_Down) {
			
			bool allow_smush = Keyboard::modifier_state_contains (ev->key.state, Keyboard::SecondaryModifier);
			fine = Keyboard::modifier_state_contains (ev->key.state, Keyboard::TertiaryModifier);
			
			if (Keyboard::modifier_state_contains (ev->key.state, Keyboard::PrimaryModifier)) {
				change_velocities (false, fine, allow_smush);
			} else {
				transpose (false, fine, allow_smush);
			}
			return true;

		} else if (ev->key.keyval == GDK_Left) {
			
			nudge_notes (false);
			return true;

		} else if (ev->key.keyval == GDK_Right) {

			nudge_notes (true);
			return true;

		} else if (ev->key.keyval == GDK_Control_L) {
			return true;

		} else if (ev->key.keyval == GDK_r) {
			/* if we're not step editing, this really doesn't matter */
			midi_view()->step_edit_rest ();
			return true;
		}

		return false;

	case GDK_KEY_RELEASE:
		if (ev->key.keyval == GDK_Alt_L || ev->key.keyval == GDK_Alt_R) {
			_mouse_state = None;
			return true;
		}
		return false;

	case GDK_BUTTON_PRESS:
		if (_mouse_state != SelectTouchDragging && ev->button.button == 1) {
			_pressed_button = ev->button.button;
			_mouse_state = Pressed;
			return true;
		}
		_pressed_button = ev->button.button;
		return true;

	case GDK_2BUTTON_PRESS:
		return true;

	case GDK_ENTER_NOTIFY:
		/* FIXME: do this on switch to note tool, too, if the pointer is already in */
		Keyboard::magic_widget_grab_focus();
		group->grab_focus();
		break;

	case GDK_MOTION_NOTIFY:
		event_x = ev->motion.x;
		event_y = ev->motion.y;
		group->w2i(event_x, event_y);

		// convert event_x to global frame
		event_frame = trackview.editor().pixel_to_frame(event_x) + _region->position();
		trackview.editor().snap_to(event_frame);
		// convert event_frame back to local coordinates relative to position
		event_frame -= _region->position();

		switch (_mouse_state) {
		case Pressed: // Drag start

			// Select drag start
			if (_pressed_button == 1 && editor.current_mouse_mode() == MouseObject) {
				group->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
						Gdk::Cursor(Gdk::FLEUR), ev->motion.time);
				last_x = event_x;
				last_y = event_y;
				drag_start_x = event_x;
				drag_start_y = event_y;

				drag_rect = new ArdourCanvas::SimpleRect(*group);
				drag_rect->property_x1() = event_x;
				drag_rect->property_y1() = event_y;
				drag_rect->property_x2() = event_x;
				drag_rect->property_y2() = event_y;
				drag_rect->property_outline_what() = 0xFF;
				drag_rect->property_outline_color_rgba()
					= ARDOUR_UI::config()->canvasvar_MidiSelectRectOutline.get();
				drag_rect->property_fill_color_rgba()
					= ARDOUR_UI::config()->canvasvar_MidiSelectRectFill.get();

				_mouse_state = SelectRectDragging;
				return true;

			// Add note drag start
			} else if (editor.current_mouse_mode() == MouseRange) {
				group->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
						Gdk::Cursor(Gdk::FLEUR), ev->motion.time);
				last_x = event_x;
				last_y = event_y;
				drag_start_x = event_x;
				drag_start_y = event_y;

				drag_rect = new ArdourCanvas::SimpleRect(*group);
				drag_rect->property_x1() = trackview.editor().frame_to_pixel(event_frame);

				drag_rect->property_y1() = midi_stream_view()->note_to_y(
						midi_stream_view()->y_to_note(event_y));
				drag_rect->property_x2() = event_x;
				drag_rect->property_y2() = drag_rect->property_y1()
				                         + floor(midi_stream_view()->note_height());
				drag_rect->property_outline_what() = 0xFF;
				drag_rect->property_outline_color_rgba() = 0xFFFFFF99;
				drag_rect->property_fill_color_rgba()    = 0xFFFFFF66;

				_mouse_state = AddDragging;
				return true;
			}

			return false;

		case SelectRectDragging: // Select drag motion
		case AddDragging: // Add note drag motion
			if (ev->motion.is_hint) {
				int t_x;
				int t_y;
				GdkModifierType state;
				gdk_window_get_pointer(ev->motion.window, &t_x, &t_y, &state);
				event_x = t_x;
				event_y = t_y;
			}

			if (_mouse_state == AddDragging)
				event_x = trackview.editor().frame_to_pixel(event_frame);

			if (drag_rect) {
				if (event_x > drag_start_x)
					drag_rect->property_x2() = event_x;
				else
					drag_rect->property_x1() = event_x;
			}

			if (drag_rect && _mouse_state == SelectRectDragging) {
				if (event_y > drag_start_y)
					drag_rect->property_y2() = event_y;
				else
					drag_rect->property_y1() = event_y;

				update_drag_selection(drag_start_x, event_x, drag_start_y, event_y);
			}

			last_x = event_x;
			last_y = event_y;

		case SelectTouchDragging:
			return false;

		default:
			break;
		}
		break;

	case GDK_BUTTON_RELEASE:
		event_x = ev->motion.x;
		event_y = ev->motion.y;
		group->w2i(event_x, event_y);
		group->ungrab(ev->button.time);
		event_frame = trackview.editor().pixel_to_frame(event_x);

		if (ev->button.button == 3) {
			return false;
		} else if (_pressed_button != 1) {
			return false;
		}
			
		switch (_mouse_state) {
		case Pressed: // Clicked
			switch (editor.current_mouse_mode()) {
			case MouseObject:
			case MouseTimeFX:
				clear_selection();
				break;
			case MouseRange:
				create_note_at(event_x, event_y, _default_note_length);
				break;
			default: 
				break;
			}
			_mouse_state = None;
			break;
		case SelectRectDragging: // Select drag done
			_mouse_state = None;
			delete drag_rect;
			drag_rect = 0;
			break;
		case AddDragging: // Add drag done
			_mouse_state = None;
			if (drag_rect->property_x2() > drag_rect->property_x1() + 2) {
				const double x      = drag_rect->property_x1();
				const double length = trackview.editor().pixel_to_frame(
				                        drag_rect->property_x2() - drag_rect->property_x1());
					
				create_note_at(x, drag_rect->property_y1(), frames_to_beats(length));
			}

			delete drag_rect;
			drag_rect = 0;
		default: break;
		}
		
	default: break;
	}

	return false;
}

void
MidiRegionView::show_list_editor ()
{
	MidiListEditor* mle = new MidiListEditor (trackview.session(), midi_region());
	mle->show ();
}

/** Add a note to the model, and the view, at a canvas (click) coordinate.
 * \param x horizontal position in pixels
 * \param y vertical position in pixels
 * \param length duration of the note in beats */
void
MidiRegionView::create_note_at(double x, double y, double length)
{
	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	MidiStreamView* const view = mtv->midi_view();

	double note = midi_stream_view()->y_to_note(y);

	assert(note >= 0.0);
	assert(note <= 127.0);

	// Start of note in frames relative to region start
	nframes64_t start_frames = snap_frame_to_frame(trackview.editor().pixel_to_frame(x));
	assert(start_frames >= 0);

	// Snap length
	length = frames_to_beats(
			snap_frame_to_frame(start_frames + beats_to_frames(length)) - start_frames);

	const boost::shared_ptr<NoteType> new_note(new NoteType(0,
			frames_to_beats(start_frames + _region->start()), length,
			(uint8_t)note, 0x40));

	view->update_note_range(new_note->note());

	MidiModel::DeltaCommand* cmd = _model->new_delta_command("add note");
	cmd->add(new_note);
	_model->apply_command(trackview.session(), cmd);

	play_midi_note (new_note);
}

void
MidiRegionView::clear_events()
{
	clear_selection();

	MidiGhostRegion* gr;
	for (std::vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
		if ((gr = dynamic_cast<MidiGhostRegion*>(*g)) != 0) {
			gr->clear_events();
		}
	}

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		delete *i;
	}

	_events.clear();
	_pgm_changes.clear();
	_sys_exes.clear();
	_optimization_iterator = _events.end();
}


void
MidiRegionView::display_model(boost::shared_ptr<MidiModel> model)
{
	_model = model;
	content_connection.disconnect ();
	content_connection = _model->ContentsChanged.connect(sigc::mem_fun(this, &MidiRegionView::redisplay_model));
	clear_events ();

	if (_enable_display) {
		redisplay_model();
	}
}
	
	
void
MidiRegionView::start_delta_command(string name)
{
	if (!_delta_command) {
		_delta_command = _model->new_delta_command(name);
	}
}

void
MidiRegionView::start_diff_command(string name)
{
	if (!_diff_command) {
		_diff_command = _model->new_diff_command(name);
	}
}

void
MidiRegionView::delta_add_note(const boost::shared_ptr<NoteType> note, bool selected, bool show_velocity)
{
	if (_delta_command) {
		_delta_command->add(note);
	}
	if (selected) {
		_marked_for_selection.insert(note);
	}
	if (show_velocity) {
		_marked_for_velocity.insert(note);
	}
}

void
MidiRegionView::delta_remove_note(ArdourCanvas::CanvasNoteEvent* ev)
{
	if (_delta_command && ev->note()) {
		_delta_command->remove(ev->note());
	}
}

void
MidiRegionView::diff_add_change (ArdourCanvas::CanvasNoteEvent* ev, 
				 MidiModel::DiffCommand::Property property,
				 uint8_t val)
{
	if (_diff_command) {
		_diff_command->change (ev->note(), property, val);
	}
}

void
MidiRegionView::diff_add_change (ArdourCanvas::CanvasNoteEvent* ev, 
				 MidiModel::DiffCommand::Property property,
				 Evoral::MusicalTime val)
{
	if (_diff_command) {
		_diff_command->change (ev->note(), property, val);
	}
}
	
void
MidiRegionView::apply_delta()
{
	if (!_delta_command) {
		return;
	}

	// Mark all selected notes for selection when model reloads
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		_marked_for_selection.insert((*i)->note());
	}
	
	_model->apply_command(trackview.session(), _delta_command);
	_delta_command = 0; 
	midi_view()->midi_track()->diskstream()->playlist_modified();

	_marked_for_selection.clear();
	_marked_for_velocity.clear();
}

void
MidiRegionView::apply_diff ()
{
	if (!_diff_command) {
		return;
	}

	_model->apply_command(trackview.session(), _diff_command);
	_diff_command = 0; 
	midi_view()->midi_track()->diskstream()->playlist_modified();

	_marked_for_velocity.clear();
}

void
MidiRegionView::apply_delta_as_subcommand()
{
	if (!_delta_command) {
		return;
	}

	// Mark all selected notes for selection when model reloads
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		_marked_for_selection.insert((*i)->note());
	}
	
	_model->apply_command_as_subcommand(trackview.session(), _delta_command);
	_delta_command = 0; 
	midi_view()->midi_track()->diskstream()->playlist_modified();

	_marked_for_selection.clear();
	_marked_for_velocity.clear();
}

void
MidiRegionView::apply_diff_as_subcommand()
{
	if (!_diff_command) {
		return;
	}

	// Mark all selected notes for selection when model reloads
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		_marked_for_selection.insert((*i)->note());
	}
	
	_model->apply_command_as_subcommand(trackview.session(), _diff_command);
	_diff_command = 0; 
	midi_view()->midi_track()->diskstream()->playlist_modified();

	_marked_for_selection.clear();
	_marked_for_velocity.clear();
}

void
MidiRegionView::abort_command()
{
	delete _delta_command;
	_delta_command = 0;
	delete _diff_command;
	_diff_command = 0;
	clear_selection();
}

CanvasNoteEvent*
MidiRegionView::find_canvas_note (boost::shared_ptr<NoteType> note)
{
	if (_optimization_iterator != _events.end()) {
		++_optimization_iterator;
	}
	
	if (_optimization_iterator != _events.end() && (*_optimization_iterator)->note() == note) {
		return *_optimization_iterator;
	} 

	for (_optimization_iterator = _events.begin(); _optimization_iterator != _events.end(); ++_optimization_iterator) {
		if ((*_optimization_iterator)->note() == note) {
			return *_optimization_iterator;
		}
	}

	return 0;
}

void
MidiRegionView::redisplay_model()
{
	// Don't redisplay the model if we're currently recording and displaying that
	if (_active_notes) {
		return;
	}

	if (!_model) {
		cerr << "MidiRegionView::redisplay_model called without a model" << endmsg;
		return;
	}

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		(*i)->invalidate ();
	}
	
	_model->read_lock();
	
	MidiModel::Notes& notes (_model->notes());
	_optimization_iterator = _events.begin();
	
	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		boost::shared_ptr<NoteType> note (*n);
		CanvasNoteEvent* cne;
		bool visible;

		if (note_in_region_range (note, visible)) {
			
			if ((cne = find_canvas_note (note)) != 0) {
				
				cne->validate ();
				
				CanvasNote* cn;
				CanvasHit* ch;
				
				if ((cn = dynamic_cast<CanvasNote*>(cne)) != 0) {
					update_note (cn);
				} else if ((ch = dynamic_cast<CanvasHit*>(cne)) != 0) {
					update_hit (ch);
				}

				if (visible) {
					cne->show ();
				} else {
					cne->hide ();
				}
				
			} else {
				
				add_note (note, visible);
			}
			
		} else {
			
			if ((cne = find_canvas_note (note)) != 0) {
				cne->validate ();
				cne->hide ();
			}
		}
	}
	
	/* remove note items that are no longer valid */
	
	for (Events::iterator i = _events.begin(); i != _events.end(); ) {
		if (!(*i)->valid ()) {
			delete *i;
			i = _events.erase (i);
		} else {
			++i;
		}
	}
	
	display_sysexes();
	display_program_changes();
	
	_model->read_unlock();
	
	_marked_for_selection.clear ();
	_marked_for_velocity.clear ();

	/* we may have caused _events to contain things out of order (e.g. if a note
	   moved earlier or later). we don't generally need them in time order, but
	   make a note that a sort is required for those cases that require it.
	*/

	_sort_needed = true;
}

void
MidiRegionView::display_program_changes()
{
	boost::shared_ptr<Evoral::Control> control = _model->control(MidiPgmChangeAutomation);
	if (!control) {
		return;
	}

	Glib::Mutex::Lock lock (control->list()->lock());

	uint8_t channel = control->parameter().channel();

	for (AutomationList::const_iterator event = control->list()->begin();
			event != control->list()->end(); ++event) {
		double event_time     = (*event)->when;
		double program_number = floor((*event)->value + 0.5);

		// Get current value of bank select MSB at time of the program change
		Evoral::Parameter bank_select_msb(MidiCCAutomation, channel, MIDI_CTL_MSB_BANK);
		boost::shared_ptr<Evoral::Control> msb_control = _model->control(bank_select_msb);
		uint8_t msb = 0;
		if (msb_control != 0) {
			msb = uint8_t(floor(msb_control->get_float(true, event_time) + 0.5));
		}

		// Get current value of bank select LSB at time of the program change
		Evoral::Parameter bank_select_lsb(MidiCCAutomation, channel, MIDI_CTL_LSB_BANK);
		boost::shared_ptr<Evoral::Control> lsb_control = _model->control(bank_select_lsb);
		uint8_t lsb = 0;
		if (lsb_control != 0) {
			lsb = uint8_t(floor(lsb_control->get_float(true, event_time) + 0.5));
		}

		MIDI::Name::PatchPrimaryKey patch_key(msb, lsb, program_number);

		boost::shared_ptr<MIDI::Name::Patch> patch = 
			MIDI::Name::MidiPatchManager::instance().find_patch(
					_model_name, _custom_device_mode, channel, patch_key);

		PCEvent program_change(event_time, uint8_t(program_number), channel);

		if (patch != 0) {
			add_pgm_change(program_change, patch->name());
		} else {
			char buf[4];
			snprintf(buf, 4, "%d", int(program_number));
			add_pgm_change(program_change, buf);
		}
	}
}

void 
MidiRegionView::display_sysexes()
{
	for (MidiModel::SysExes::const_iterator i = _model->sysexes().begin(); i != _model->sysexes().end(); ++i) {
		Evoral::MusicalTime time = (*i)->time();
		assert(time >= 0);
		
		ostringstream str;
		str << hex;
		for (uint32_t b = 0; b < (*i)->size(); ++b) {
			str << int((*i)->buffer()[b]);
			if (b != (*i)->size() -1) {
				str << " ";
			}
		}
		string text = str.str();
		
		ArdourCanvas::Group* const group = (ArdourCanvas::Group*)get_canvas_group();

		const double x = trackview.editor().frame_to_pixel(beats_to_frames(time));
		
		double height = midi_stream_view()->contents_height();
		
		boost::shared_ptr<CanvasSysEx> sysex = boost::shared_ptr<CanvasSysEx>(
				new CanvasSysEx(*this, *group, text, height, x, 1.0));
		
		// Show unless program change is beyond the region bounds
		if (time - _region->start() >= _region->length() || time < _region->start()) {
			sysex->hide();
		} else {
			sysex->show();
		}
		
		_sys_exes.push_back(sysex);
	}
}


MidiRegionView::~MidiRegionView ()
{
	in_destructor = true;

	RegionViewGoingAway (this); /* EMIT_SIGNAL */

	if (_active_notes) {
		end_write();
	}

	_selection.clear();
	clear_events();
	delete _note_group;
	delete _delta_command;
}

void
MidiRegionView::region_resized (Change what_changed)
{
	RegionView::region_resized(what_changed);
	
	if (what_changed & ARDOUR::PositionChanged) {
		set_duration(_region->length(), 0);
		if (_enable_display) {
			redisplay_model();
		}
	} 
}

void
MidiRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);
	assert(_pixel_width == pixel_width);

	if (_enable_display) {
		redisplay_model();
	}
}

void
MidiRegionView::set_height (double height)
{
	static const double FUDGE = 2.0;
	const double old_height = _height;
	RegionView::set_height(height);
	_height = height - FUDGE;
	
	apply_note_range(midi_stream_view()->lowest_note(),
	                 midi_stream_view()->highest_note(),
	                 height != old_height + FUDGE);
	
	if (name_pixbuf) {
		name_pixbuf->raise_to_top();
	}
}


/** Apply the current note range from the stream view
 * by repositioning/hiding notes as necessary
 */
void
MidiRegionView::apply_note_range (uint8_t min, uint8_t max, bool force)
{
	if (!_enable_display) {
		return;
	}

	if (!force && _current_range_min == min && _current_range_max == max) {
		return;
	}

	_current_range_min = min;
	_current_range_max = max;

	for (Events::const_iterator i = _events.begin(); i != _events.end(); ++i) {
		CanvasNoteEvent* event = *i;
		boost::shared_ptr<NoteType> note (event->note());

		if (note->note() < _current_range_min || 
		    note->note() > _current_range_max) {
			event->hide();
		} else {
			event->show();
		}
		
		if (CanvasNote* cnote = dynamic_cast<CanvasNote*>(event)) {

			const double y1 = midi_stream_view()->note_to_y(note->note());
			const double y2 = y1 + floor(midi_stream_view()->note_height());
			
			cnote->property_y1() = y1;
			cnote->property_y2() = y2;

		} else if (CanvasHit* chit = dynamic_cast<CanvasHit*>(event)) {

			double x = trackview.editor().frame_to_pixel(
				beats_to_frames(note->time()) - _region->start());
			const double diamond_size = midi_stream_view()->note_height() / 2.0;
			double y = midi_stream_view()->note_to_y(event->note()->note()) 
				+ ((diamond_size-2.0) / 4.0);
			
			chit->set_height (diamond_size);
			chit->move (x - chit->x1(), y - chit->y1());
			chit->show ();
		}
	}
}

GhostRegion*
MidiRegionView::add_ghost (TimeAxisView& tv)
{
	CanvasNote* note;

	double unit_position = _region->position () / samples_per_unit;
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&tv);
	MidiGhostRegion* ghost;

	if (mtv && mtv->midi_view()) {
		/* if ghost is inserted into midi track, use a dedicated midi ghost canvas group
		   to allow having midi notes on top of note lines and waveforms.
		 */
		ghost = new MidiGhostRegion (*mtv->midi_view(), trackview, unit_position);
	} else {
		ghost = new MidiGhostRegion (tv, trackview, unit_position);
	}

	ghost->set_height ();
	ghost->set_duration (_region->length() / samples_per_unit);
	ghosts.push_back (ghost);

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((note = dynamic_cast<CanvasNote*>(*i)) != 0) {
			ghost->add_note(note);
		}
	}

	ghost->GoingAway.connect (mem_fun(*this, &MidiRegionView::remove_ghost));

	return ghost;
}


/** Begin tracking note state for successive calls to add_event
 */
void
MidiRegionView::begin_write()
{
	assert(!_active_notes);
	_active_notes = new CanvasNote*[128];
	for (unsigned i=0; i < 128; ++i) {
		_active_notes[i] = 0;
	}
}


/** Destroy note state for add_event
 */
void
MidiRegionView::end_write()
{
	delete[] _active_notes;
	_active_notes = 0;
	_marked_for_selection.clear();
	_marked_for_velocity.clear();
}


/** Resolve an active MIDI note (while recording).
 */
void
MidiRegionView::resolve_note(uint8_t note, double end_time)
{
	if (midi_view()->note_mode() != Sustained) {
		return;
	}

	if (_active_notes && _active_notes[note]) {
		const nframes64_t end_time_frames = beats_to_frames(end_time);
		_active_notes[note]->property_x2() = trackview.editor().frame_to_pixel(end_time_frames);
		_active_notes[note]->property_outline_what() = (guint32) 0xF; // all edges
		_active_notes[note] = 0;
	}
}


/** Extend active notes to rightmost edge of region (if length is changed)
 */
void
MidiRegionView::extend_active_notes()
{
	if (!_active_notes) {
		return;
	}

	for (unsigned i=0; i < 128; ++i) {
		if (_active_notes[i]) {
			_active_notes[i]->property_x2() = trackview.editor().frame_to_pixel(_region->length());
		}
	}
}

void 
MidiRegionView::play_midi_note(boost::shared_ptr<NoteType> note)
{
	if (!trackview.editor().sound_notes()) {
		return;
	}

	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);
	assert(route_ui);
	
	route_ui->midi_track()->write_immediate_event(
			note->on_event().size(), note->on_event().buffer());
	
	const double note_length_beats = (note->off_event().time() - note->on_event().time());
	nframes_t note_length_ms = beats_to_frames(note_length_beats)
			* (1000 / (double)route_ui->session().nominal_frame_rate());
	Glib::signal_timeout().connect(bind(mem_fun(this, &MidiRegionView::play_midi_note_off), note),
			note_length_ms, G_PRIORITY_DEFAULT);
}

bool
MidiRegionView::play_midi_note_off(boost::shared_ptr<NoteType> note)
{
	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);
	assert(route_ui);
	
	route_ui->midi_track()->write_immediate_event(
			note->off_event().size(), note->off_event().buffer());

	return false;
}

bool
MidiRegionView::note_in_region_range(const boost::shared_ptr<NoteType> note, bool& visible) const
{
	const nframes64_t note_start_frames = beats_to_frames(note->time());

	bool outside = (note_start_frames - _region->start() >= _region->length()) || 
		(note_start_frames < _region->start());

	visible = (note->note() >= midi_stream_view()->lowest_note()) &&
		(note->note() <= midi_stream_view()->highest_note());

	return !outside;
}

void
MidiRegionView::update_note (CanvasNote* ev)
{
	boost::shared_ptr<NoteType> note = ev->note();

	const nframes64_t note_start_frames = beats_to_frames(note->time());
	const nframes64_t note_end_frames   = beats_to_frames(note->end_time());

	const double x = trackview.editor().frame_to_pixel(note_start_frames - _region->start());
	const double y1 = midi_stream_view()->note_to_y(note->note());
	const double note_endpixel = 
		trackview.editor().frame_to_pixel(note_end_frames - _region->start());

	ev->property_x1() = x;
	ev->property_y1() = y1;
	if (note->length() > 0) {
		ev->property_x2() = note_endpixel;
	} else {
		ev->property_x2() = trackview.editor().frame_to_pixel(_region->length());
	}
	ev->property_y2() = y1 + floor(midi_stream_view()->note_height());
	
	if (note->length() == 0) {
		if (_active_notes) {
			assert(note->note() < 128);
			// If this note is already active there's a stuck note,
			// finish the old note rectangle
			if (_active_notes[note->note()]) {
				CanvasNote* const old_rect = _active_notes[note->note()];
				boost::shared_ptr<NoteType> old_note = old_rect->note();
				old_rect->property_x2() = x;
				old_rect->property_outline_what() = (guint32) 0xF;
			}
			_active_notes[note->note()] = ev;
		}
		/* outline all but right edge */
		ev->property_outline_what() = (guint32) (0x1 & 0x4 & 0x8);
	} else {
		/* outline all edges */
		ev->property_outline_what() = (guint32) 0xF;
	}
}

void
MidiRegionView::update_hit (CanvasHit* ev)
{
	boost::shared_ptr<NoteType> note = ev->note();

	const nframes64_t note_start_frames = beats_to_frames(note->time());
	const double x = trackview.editor().frame_to_pixel(note_start_frames - _region->start());
	const double diamond_size = midi_stream_view()->note_height() / 2.0;
	const double y = midi_stream_view()->note_to_y(note->note()) + ((diamond_size-2) / 4.0);

	ev->move_event (x, y);
}

/** Add a MIDI note to the view (with length).
 *
 * If in sustained mode, notes with length 0 will be considered active
 * notes, and resolve_note should be called when the corresponding note off
 * event arrives, to properly display the note.
 */
void
MidiRegionView::add_note(const boost::shared_ptr<NoteType> note, bool visible)
{
	CanvasNoteEvent* event = 0;
	
	assert(note->time() >= 0);
	assert(midi_view()->note_mode() == Sustained || midi_view()->note_mode() == Percussive);

	ArdourCanvas::Group* const group = (ArdourCanvas::Group*)get_canvas_group();

	if (midi_view()->note_mode() == Sustained) {
		
		CanvasNote* ev_rect = new CanvasNote(*this, *group, note);

		update_note (ev_rect);

		event = ev_rect;

		MidiGhostRegion* gr;

		for (std::vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
			if ((gr = dynamic_cast<MidiGhostRegion*>(*g)) != 0) {
				gr->add_note(ev_rect);
			}
		}

	} else if (midi_view()->note_mode() == Percussive) {

		const double diamond_size = midi_stream_view()->note_height() / 2.0;

		CanvasHit* ev_diamond = new CanvasHit(*this, *group, diamond_size, note);

		update_hit (ev_diamond);

		event = ev_diamond;

	} else {
		event = 0;
	}

	if (event) {
		if (_marked_for_selection.find(note) != _marked_for_selection.end()) {
			note_selected(event, true);
		} 

		if (_marked_for_velocity.find(note) != _marked_for_velocity.end()) {
			event->show_velocity();
		}
		event->on_channel_selection_change(_last_channel_selection);
		_events.push_back(event);

		if (visible) {
			event->show();
		} else {
			event->hide ();
		}
	}
}

void
MidiRegionView::add_note (uint8_t channel, uint8_t number, uint8_t velocity, 
			  Evoral::MusicalTime pos, Evoral::MusicalTime len)
{
	boost::shared_ptr<NoteType> new_note (new NoteType (channel, pos, len, number, velocity));
	
	start_delta_command (_("step add"));
	delta_add_note (new_note, true, false);
	apply_delta();

	/* potentially extend region to hold new note */

	nframes64_t end_frame = _region->position() + beats_to_frames (new_note->end_time());
	nframes64_t region_end = _region->position() + _region->length() - 1;

	if (end_frame > region_end) {
		_region->set_length (end_frame, this);
	} else {
		redisplay_model ();
	}
}

void
MidiRegionView::add_pgm_change(PCEvent& program, const string& displaytext)
{
	assert(program.time >= 0);
	
	ArdourCanvas::Group* const group = (ArdourCanvas::Group*)get_canvas_group();
	const double x = trackview.editor().frame_to_pixel(beats_to_frames(program.time));
	
	double height = midi_stream_view()->contents_height();
	
	boost::shared_ptr<CanvasProgramChange> pgm_change = boost::shared_ptr<CanvasProgramChange>(
			new CanvasProgramChange(*this, *group,
					displaytext, 
					height, 
					x, 1.0, 
					_model_name, 
					_custom_device_mode, 
					program.time, program.channel, program.value));
	
	// Show unless program change is beyond the region bounds
	if (program.time - _region->start() >= _region->length() || program.time < _region->start()) {
		pgm_change->hide();
	} else {
		pgm_change->show();
	}
	
	_pgm_changes.push_back(pgm_change);
}

void
MidiRegionView::get_patch_key_at(double time, uint8_t channel, MIDI::Name::PatchPrimaryKey& key)
{
	cerr << "getting patch key at " << time << " for channel " << channel << endl;
	Evoral::Parameter bank_select_msb(MidiCCAutomation, channel, MIDI_CTL_MSB_BANK);
	boost::shared_ptr<Evoral::Control> msb_control = _model->control(bank_select_msb);
	float msb = -1.0;
	if (msb_control != 0) {
		msb = int(msb_control->get_float(true, time));
		cerr << "got msb " << msb;
	}

	Evoral::Parameter bank_select_lsb(MidiCCAutomation, channel, MIDI_CTL_LSB_BANK);
	boost::shared_ptr<Evoral::Control> lsb_control = _model->control(bank_select_lsb);
	float lsb = -1.0;
	if (lsb_control != 0) {
		lsb = lsb_control->get_float(true, time);
		cerr << " got lsb " << lsb;
	}
	
	Evoral::Parameter program_change(MidiPgmChangeAutomation, channel, 0);
	boost::shared_ptr<Evoral::Control> program_control = _model->control(program_change);
	float program_number = -1.0;
	if (program_control != 0) {
		program_number = program_control->get_float(true, time);
		cerr << " got program " << program_number << endl;
	}
	
	key.msb = (int) floor(msb + 0.5);
	key.lsb = (int) floor(lsb + 0.5);
	key.program_number = (int) floor(program_number + 0.5);
	assert(key.is_sane());
}


void 
MidiRegionView::alter_program_change(PCEvent& old_program, const MIDI::Name::PatchPrimaryKey& new_patch)
{
	// TODO: Get the real event here and alter them at the original times
	Evoral::Parameter bank_select_msb(MidiCCAutomation, old_program.channel, MIDI_CTL_MSB_BANK);
	boost::shared_ptr<Evoral::Control> msb_control = _model->control(bank_select_msb);
	if (msb_control != 0) {
		msb_control->set_float(float(new_patch.msb), true, old_program.time);
	}

	// TODO: Get the real event here and alter them at the original times
	Evoral::Parameter bank_select_lsb(MidiCCAutomation, old_program.channel, MIDI_CTL_LSB_BANK);
	boost::shared_ptr<Evoral::Control> lsb_control = _model->control(bank_select_lsb);
	if (lsb_control != 0) {
		lsb_control->set_float(float(new_patch.lsb), true, old_program.time);
	}
	
	Evoral::Parameter program_change(MidiPgmChangeAutomation, old_program.channel, 0);
	boost::shared_ptr<Evoral::Control> program_control = _model->control(program_change);
	
	assert(program_control != 0);
	program_control->set_float(float(new_patch.program_number), true, old_program.time);
	
	redisplay_model();
}

void
MidiRegionView::program_selected(CanvasProgramChange& program, const MIDI::Name::PatchPrimaryKey& new_patch)
{
	PCEvent program_change_event(program.event_time(), program.program(), program.channel());
	alter_program_change(program_change_event, new_patch);
}

void 
MidiRegionView::previous_program(CanvasProgramChange& program)
{
	MIDI::Name::PatchPrimaryKey key;
	get_patch_key_at(program.event_time(), program.channel(), key);
	
	boost::shared_ptr<MIDI::Name::Patch> patch = 
		MIDI::Name::MidiPatchManager::instance().previous_patch(
				_model_name,
				_custom_device_mode, 
				program.channel(), 
				key);
	
	PCEvent program_change_event(program.event_time(), program.program(), program.channel());
	if (patch) {
		alter_program_change(program_change_event, patch->patch_primary_key());
	}
}

void 
MidiRegionView::next_program(CanvasProgramChange& program)
{
	MIDI::Name::PatchPrimaryKey key;
	get_patch_key_at(program.event_time(), program.channel(), key);
	
	boost::shared_ptr<MIDI::Name::Patch> patch = 
		MIDI::Name::MidiPatchManager::instance().next_patch(
				_model_name,
				_custom_device_mode, 
				program.channel(), 
				key);	

	PCEvent program_change_event(program.event_time(), program.program(), program.channel());
	if (patch) {
		alter_program_change(program_change_event, patch->patch_primary_key());
	}
}

void
MidiRegionView::delete_selection()
{
	if (_selection.empty()) {
		return;
	}

	start_delta_command (_("delete selection"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->selected()) {
			_delta_command->remove((*i)->note());
		}
	}

	_selection.clear();

	apply_delta ();
}

void
MidiRegionView::clear_selection_except(ArdourCanvas::CanvasNoteEvent* ev)
{
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->selected() && (*i) != ev) {
			(*i)->selected(false);
			(*i)->hide_velocity();
		}
	}

	_selection.clear();
}

void
MidiRegionView::unique_select(ArdourCanvas::CanvasNoteEvent* ev)
{
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		if ((*i) != ev) {

			Selection::iterator tmp = i;
			++tmp;

			(*i)->selected (false);
			_selection.erase (i);

			i = tmp;

		} else {
			++i;
		}
	}

	/* don't bother with removing this regionview from the editor selection,
	   since we're about to add another note, and thus put/keep this
	   regionview in the editor selection.
	*/

	if (!ev->selected()) {
		add_to_selection (ev);
	}
}

void
MidiRegionView::note_selected(ArdourCanvas::CanvasNoteEvent* ev, bool add, bool extend)
{
	if (!add) {
		clear_selection_except(ev);
	}

	if (!extend) {

		if (!ev->selected()) {
			add_to_selection (ev);
		}

	} else {
		/* find end of latest note selected, select all between that and the start of "ev" */

		Evoral::MusicalTime earliest = DBL_MAX;
		Evoral::MusicalTime latest = 0;

		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if ((*i)->note()->end_time() > latest) {
				latest = (*i)->note()->end_time();
			} 
			if ((*i)->note()->time() < earliest) {
				earliest = (*i)->note()->time();
			}
		}

		if (ev->note()->end_time() > latest) {
			latest = ev->note()->end_time();
		}

		if (ev->note()->time() < earliest) {
			earliest = ev->note()->time();
		}
		
		for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {		
			
			/* find notes entirely within OR spanning the earliest..latest range */
			
			if (((*i)->note()->time() >= earliest && (*i)->note()->end_time() <= latest) ||
			    ((*i)->note()->time() <= earliest && (*i)->note()->end_time() >= latest)) {
				add_to_selection (*i);
			} 			

#if 0
			/* if events were guaranteed to be time sorted, we could do this.
			   but as of sept 10th 2009, they no longer are.
			*/
			
			if ((*i)->note()->time() > latest) {
				break;
			}
#endif
		}
	}
}

void
MidiRegionView::note_deselected(ArdourCanvas::CanvasNoteEvent* ev)
{
	remove_from_selection (ev);
}

void
MidiRegionView::update_drag_selection(double x1, double x2, double y1, double y2)
{
	if (x1 > x2) {
		swap (x1, x2);
	}

	if (y1 > y2) {
		swap (y1, y2);
	}

	// TODO: Make this faster by storing the last updated selection rect, and only
	// adjusting things that are in the area that appears/disappeared.
	// We probably need a tree to be able to find events in O(log(n)) time.

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {

		/* check if any corner of the note is inside the rect
		   
		   Notes:
		     1) this is computing "touched by", not "contained by" the rect.
		     2) this does not require that events be sorted in time.
		 */

		const double ix1 = (*i)->x1();
		const double ix2 = (*i)->x2();
		const double iy1 = (*i)->y1();
		const double iy2 = (*i)->y2();

		if ((ix1 >= x1 && ix1 <= x2 && iy1 >= y1 && iy1 <= y2) ||
		    (ix1 >= x1 && ix1 <= x2 && iy2 >= y1 && iy2 <= y2) ||
		    (ix2 >= x1 && ix2 <= x2 && iy1 >= y1 && iy1 <= y2) ||
		    (ix2 >= x1 && ix2 <= x2 && iy2 >= y1 && iy2 <= y2)) {

			// Inside rectangle
			if (!(*i)->selected()) {
				add_to_selection (*i);
			}
		} else if ((*i)->selected()) {
			// Not inside rectangle
			remove_from_selection (*i);
		}
	}
}

void
MidiRegionView::remove_from_selection (CanvasNoteEvent* ev)
{
	Selection::iterator i = _selection.find (ev);

	if (i != _selection.end()) {
		_selection.erase (i);
	}

	ev->selected (false);
	ev->hide_velocity ();
	
	if (_selection.empty()) {
		PublicEditor& editor (trackview.editor());
		editor.get_selection().remove (this);
	}
}

void
MidiRegionView::add_to_selection (CanvasNoteEvent* ev)
{
	bool add_mrv_selection = false;

	if (_selection.empty()) {
		add_mrv_selection = true;
	}

	if (_selection.insert (ev).second) {
		ev->selected (true);
		play_midi_note ((ev)->note());
	}

	if (add_mrv_selection) {
		PublicEditor& editor (trackview.editor());
		editor.get_selection().add (this);
	}
}

void
MidiRegionView::move_selection(double dx, double dy)
{
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->move_event(dx, dy);
	}
}

void
MidiRegionView::note_dropped(CanvasNoteEvent *, double dt, int8_t dnote)
{
	assert (!_selection.empty());

	uint8_t lowest_note_in_selection  = 127;
	uint8_t highest_note_in_selection = 0;
	uint8_t highest_note_difference = 0;

	// find highest and lowest notes first

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		uint8_t pitch = (*i)->note()->note();
		lowest_note_in_selection  = std::min(lowest_note_in_selection,  pitch);
		highest_note_in_selection = std::max(highest_note_in_selection, pitch);
	}

	/*
	cerr << "dnote: " << (int) dnote << endl;
	cerr << "lowest note (streamview): " << int(midi_stream_view()->lowest_note()) 
	     << " highest note (streamview): " << int(midi_stream_view()->highest_note()) << endl;
	cerr << "lowest note (selection): " << int(lowest_note_in_selection) << " highest note(selection): " 
	     << int(highest_note_in_selection) << endl;
	cerr << "selection size: " << _selection.size() << endl;
	cerr << "Highest note in selection: " << (int) highest_note_in_selection << endl;
	*/

	// Make sure the note pitch does not exceed the MIDI standard range
	if (highest_note_in_selection + dnote > 127) {
		highest_note_difference = highest_note_in_selection - 127;
	}
	
	start_diff_command(_("move notes"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end() ; ++i) {

		nframes64_t start_frames = beats_to_frames((*i)->note()->time());

		if (dt >= 0) {
			start_frames += snap_frame_to_frame(trackview.editor().pixel_to_frame(dt));
		} else {
			start_frames -= snap_frame_to_frame(trackview.editor().pixel_to_frame(-dt));
		}

		Evoral::MusicalTime new_time = frames_to_beats(start_frames);

		if (new_time < 0) {
			continue;
		}

		diff_add_change (*i, MidiModel::DiffCommand::StartTime, new_time);

		uint8_t original_pitch = (*i)->note()->note();
		uint8_t new_pitch      = original_pitch + dnote - highest_note_difference;

		// keep notes in standard midi range
		clamp_to_0_127(new_pitch);
		
		// keep original pitch if note is dragged outside valid midi range
		if ((original_pitch != 0 && new_pitch == 0)
				|| (original_pitch != 127 && new_pitch == 127)) {
			new_pitch = original_pitch;
		}

		lowest_note_in_selection  = std::min(lowest_note_in_selection,  new_pitch);
		highest_note_in_selection = std::max(highest_note_in_selection, new_pitch);

		diff_add_change (*i, MidiModel::DiffCommand::NoteNumber, new_pitch);
	}

	apply_diff();
	
	// care about notes being moved beyond the upper/lower bounds on the canvas
	if (lowest_note_in_selection  < midi_stream_view()->lowest_note() ||
	    highest_note_in_selection > midi_stream_view()->highest_note()) {
		midi_stream_view()->set_note_range(MidiStreamView::ContentsRange);
	}
}

nframes64_t
MidiRegionView::snap_pixel_to_frame(double x)
{
	PublicEditor& editor = trackview.editor();
	// x is region relative, convert it to global absolute frames
	nframes64_t frame = editor.pixel_to_frame(x) + _region->position();
	editor.snap_to(frame);
	return frame - _region->position(); // convert back to region relative
}

nframes64_t
MidiRegionView::snap_frame_to_frame(nframes64_t x)
{
	PublicEditor& editor = trackview.editor();
	// x is region relative, convert it to global absolute frames
	nframes64_t frame = x + _region->position();
	editor.snap_to(frame);
	return frame - _region->position(); // convert back to region relative
}

double
MidiRegionView::snap_to_pixel(double x)
{
	return (double) trackview.editor().frame_to_pixel(snap_pixel_to_frame(x));
}

double
MidiRegionView::get_position_pixels()
{
	nframes64_t region_frame = get_position();
	return trackview.editor().frame_to_pixel(region_frame);
}

double
MidiRegionView::get_end_position_pixels()
{
	nframes64_t frame = get_position() + get_duration ();
	return trackview.editor().frame_to_pixel(frame);
}

nframes64_t
MidiRegionView::beats_to_frames(double beats) const
{
	return _time_converter.to(beats);
}

double
MidiRegionView::frames_to_beats(nframes64_t frames) const
{
	return _time_converter.from(frames);
}

void
MidiRegionView::begin_resizing (bool /*at_front*/)
{
	_resize_data.clear();

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		CanvasNote *note = dynamic_cast<CanvasNote *> (*i);

		// only insert CanvasNotes into the map
		if (note) {
			NoteResizeData *resize_data = new NoteResizeData();
			resize_data->canvas_note = note;

			// create a new SimpleRect from the note which will be the resize preview
			SimpleRect *resize_rect = new SimpleRect(
					*group, note->x1(), note->y1(), note->x2(), note->y2());

			// calculate the colors: get the color settings
			uint32_t fill_color = UINT_RGBA_CHANGE_A(
					ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get(),
					128);

			// make the resize preview notes more transparent and bright
			fill_color = UINT_INTERPOLATE(fill_color, 0xFFFFFF40, 0.5);

			// calculate color based on note velocity
			resize_rect->property_fill_color_rgba() = UINT_INTERPOLATE(
					CanvasNoteEvent::meter_style_fill_color(note->note()->velocity()),
					fill_color,
					0.85);

			resize_rect->property_outline_color_rgba() = CanvasNoteEvent::calculate_outline(
					ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get());

			resize_data->resize_rect = resize_rect;
			_resize_data.push_back(resize_data);
		}
	}
}

void
MidiRegionView::update_resizing (bool at_front, double delta_x, bool relative)
{
	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		SimpleRect* resize_rect = (*i)->resize_rect;
		CanvasNote* canvas_note = (*i)->canvas_note;
		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x1() + delta_x;
			} else {
				// x is in track relative, transform it to region relative
				current_x = delta_x - get_position_pixels();
			}
		} else {
			if (relative) {
				current_x = canvas_note->x2() + delta_x;
			} else {
				// x is in track relative, transform it to region relative
				current_x = delta_x - get_end_position_pixels ();
			}
		}
		
		if (at_front) {
			resize_rect->property_x1() = snap_to_pixel(current_x);
			resize_rect->property_x2() = canvas_note->x2();
		} else {
			resize_rect->property_x2() = snap_to_pixel(current_x);
			resize_rect->property_x1() = canvas_note->x1();
		}
	}
}

void
MidiRegionView::commit_resizing (bool at_front, double delta_x, bool relative)
{
	start_diff_command(_("resize notes"));

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		CanvasNote*  canvas_note = (*i)->canvas_note;
		SimpleRect*  resize_rect = (*i)->resize_rect;
		const double region_start = get_position_pixels();
		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x1() + delta_x;
			} else {
				// x is in track relative, transform it to region relative
				current_x = region_start + delta_x;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x2() + delta_x;
			} else {
				// x is in track relative, transform it to region relative
				current_x = region_start + delta_x;
			}
		}
		
		current_x = snap_pixel_to_frame (current_x);
		current_x = frames_to_beats (current_x);

		if (at_front && current_x < canvas_note->note()->end_time()) {
			diff_add_change (canvas_note, MidiModel::DiffCommand::StartTime, current_x);

			double len = canvas_note->note()->time() - current_x;
			len += canvas_note->note()->length();

			if (len > 0) {
				/* XXX convert to beats */
				diff_add_change (canvas_note, MidiModel::DiffCommand::Length, len);
			}
		}

		if (!at_front) {
			double len = current_x - canvas_note->note()->time();

			if (len > 0) {
				/* XXX convert to beats */
				diff_add_change (canvas_note, MidiModel::DiffCommand::Length, len);
			}
		}

		delete resize_rect;
		delete (*i);
	}

	_resize_data.clear();
	apply_diff();
}

void
MidiRegionView::change_note_velocity(CanvasNoteEvent* event, int8_t velocity, bool relative)
{
	uint8_t new_velocity;

	if (relative) {
		new_velocity = event->note()->velocity() + velocity;
		clamp_to_0_127(new_velocity);
	} else {
		new_velocity = velocity;
	}

	diff_add_change (event, MidiModel::DiffCommand::Velocity, new_velocity);
}

void
MidiRegionView::change_note_note (CanvasNoteEvent* event, int8_t note, bool relative)
{
	uint8_t new_note;

	if (relative) {
		new_note = event->note()->note() + note;
	} else {
		new_note = note;
	}

	clamp_to_0_127 (new_note);
	diff_add_change (event, MidiModel::DiffCommand::NoteNumber, new_note);
}

void
MidiRegionView::trim_note (CanvasNoteEvent* event, Evoral::MusicalTime front_delta, Evoral::MusicalTime end_delta)
{
	bool change_start = false;
	bool change_length = false;
	Evoral::MusicalTime new_start;
	Evoral::MusicalTime new_length;

	/* NOTE: the semantics of the two delta arguments are slightly subtle:

	   front_delta: if positive - move the start of the note later in time (shortening it)
	                if negative - move the start of the note earlier in time (lengthening it)

	   end_delta:   if positive - move the end of the note later in time (lengthening it)
	                if negative - move the end of the note earlier in time (shortening it)
	 */

	if (front_delta) {
		if (front_delta < 0) {

			if (event->note()->time() < -front_delta) {
				new_start = 0;
			} else {
				new_start = event->note()->time() + front_delta; // moves earlier
			}

			/* start moved toward zero, so move the end point out to where it used to be.
			   Note that front_delta is negative, so this increases the length.
			*/

			new_length = event->note()->length() - front_delta;
			change_start = true;
			change_length = true;

		} else {

			Evoral::MusicalTime new_pos = event->note()->time() + front_delta;
			
			if (new_pos < event->note()->end_time()) {
				new_start = event->note()->time() + front_delta;
				/* start moved toward the end, so move the end point back to where it used to be */
				new_length = event->note()->length() - front_delta; 
				change_start = true;
				change_length = true;
			}
		}

	}

	if (end_delta) {
		bool can_change = true;
		if (end_delta < 0) {
			if (event->note()->length() < -end_delta) {
				can_change = false;
			}
		} 

		if (can_change) {
			new_length = event->note()->length() + end_delta;
			change_length = true;
		}
	}

	if (change_start) {
		diff_add_change (event, MidiModel::DiffCommand::StartTime, new_start);
	}

	if (change_length) {
		diff_add_change (event, MidiModel::DiffCommand::Length, new_length);
	}
}

void
MidiRegionView::change_note_time (CanvasNoteEvent* event, Evoral::MusicalTime delta, bool relative)
{
	Evoral::MusicalTime new_time;

	if (relative) {
		if (delta < 0.0) {
			if (event->note()->time() < -delta) {
				new_time = 0;
			} else {
				new_time = event->note()->time() + delta;
			} 
		} else {
			new_time = event->note()->time() + delta;
		}
	} else {
		new_time = delta;
	}

	diff_add_change (event, MidiModel::DiffCommand::StartTime, new_time);
}

void
MidiRegionView::change_velocities (bool up, bool fine, bool allow_smush)
{
	int8_t delta;

	if (_selection.empty()) {
		return;
	}

	if (fine) {
		delta = 1;
	} else {
		delta = 10;
	}

	if (!up) {
		delta = -delta;
	}

	if (!allow_smush) {
		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if ((*i)->note()->velocity() + delta == 0 || (*i)->note()->velocity() + delta == 127) {
				return;
			}
		}
	}

	start_diff_command(_("change velocities"));
	
	for (Selection::iterator i = _selection.begin(); i != _selection.end();) {
		Selection::iterator next = i;
		++next;
		change_note_velocity (*i, delta, true);
		i = next;
	}
	
	apply_diff();
}


void
MidiRegionView::transpose (bool up, bool fine, bool allow_smush)
{
	if (_selection.empty()) {
		return;
	}

	int8_t delta;
	
	if (fine) {
		delta = 1;
	} else {
		delta = 12;
	}

	if (!up) {
		delta = -delta;
	}

	if (!allow_smush) {
		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if (!up) {
				if ((int8_t) (*i)->note()->note() + delta <= 0) {
					return;
				}
			} else {
				if ((int8_t) (*i)->note()->note() + delta > 127) {
					return;
				}
			}
		}
	}

	start_diff_command (_("transpose"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_note (*i, delta, true);
		i = next;
	}

	apply_diff ();
}

void
MidiRegionView::change_note_lengths (bool fine, bool shorter, bool start, bool end)
{
	Evoral::MusicalTime delta;

	if (fine) {
		delta = 1.0/128.0;
	} else {
		/* grab the current grid distance */
		bool success;
		delta = trackview.editor().get_grid_type_as_beats (success, _region->position());
		if (!success) {
			/* XXX cannot get grid type as beats ... should always be possible ... FIX ME */
			cerr << "Grid type not available as beats - TO BE FIXED\n";
			return;
		}
	}

	if (shorter) {
		delta = -delta;
	}
	
	start_diff_command (_("change note lengths"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		
		/* note the negation of the delta for start */

		trim_note (*i, (start ? -delta : 0), (end ? delta : 0));
		i = next;
	}

	apply_diff ();

}

void
MidiRegionView::nudge_notes (bool forward)
{
	if (_selection.empty()) {
		return;
	}

	/* pick a note as the point along the timeline to get the nudge distance. 
	   its not necessarily the earliest note, so we may want to pull the notes out 
	   into a vector and sort before using the first one.
	*/

	nframes64_t ref_point = _region->position() + beats_to_frames ((*(_selection.begin()))->note()->time());
	nframes64_t unused;
	nframes64_t distance;

	if ((distance = trackview.editor().get_nudge_distance (ref_point, unused)) == 0) {

		/* no nudge distance set - use grid */

		nframes64_t next_pos = ref_point;
		
		if (forward) {
			/* XXX need check on max_frames, but that needs max_frames64 or something */
			next_pos += 1;
		} else { 
			if (next_pos == 0) {
				return;
			}
			next_pos -= 1;
		}
		
		cerr << "ref point was " << ref_point << " next was " << next_pos;
		trackview.editor().snap_to (next_pos, (forward ? 1 : -1), false);
		distance = ref_point - next_pos;
		cerr << " final is " << next_pos << " distance = " << distance << endl;
	} 
		
	if (distance == 0) {
		return;
	}

	Evoral::MusicalTime delta = frames_to_beats (fabs (distance));

	if (!forward) {
		delta = -delta;
	}

	start_diff_command (_("nudge"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_time (*i, delta, true);
		i = next;
	}

	apply_diff ();
}

void
MidiRegionView::change_channel(uint8_t channel)
{
	start_diff_command(_("change channel"));
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		diff_add_change (*i, MidiModel::DiffCommand::Channel, channel);
	}
	apply_diff();
}


void
MidiRegionView::note_entered(ArdourCanvas::CanvasNoteEvent* ev)
{
	if (_mouse_state == SelectTouchDragging) {
		note_selected(ev, true);
	}

	PublicEditor& editor (trackview.editor());
	char buf[4];
	snprintf (buf, sizeof (buf), "%d", (int) ev->note()->note());
	//editor.show_verbose_canvas_cursor_with (Evoral::midi_note_name (ev->note()->note()));
	editor.show_verbose_canvas_cursor_with (buf);
}

void
MidiRegionView::note_left (ArdourCanvas::CanvasNoteEvent*)
{
	PublicEditor& editor (trackview.editor());
	editor.hide_verbose_canvas_cursor ();
}
	

void
MidiRegionView::switch_source(boost::shared_ptr<Source> src)
{
	boost::shared_ptr<MidiSource> msrc = boost::dynamic_pointer_cast<MidiSource>(src);
	if (msrc)
		display_model(msrc->model());
}

void
MidiRegionView::set_frame_color()
{
	if (frame) {
		if (_selected && should_show_selection) {
			frame->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_SelectedFrameBase.get();
		} else {
			frame->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiFrameBase.get();
		}
	}
}

void 
MidiRegionView::midi_channel_mode_changed(ChannelMode mode, uint16_t mask)
{
	switch (mode) {
	case AllChannels:
	case FilterChannels:
		_force_channel = -1;
		break;
	case ForceChannel:
		_force_channel = mask;
		mask = 0xFFFF; // Show all notes as active (below)
	};

	// Update notes for selection
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		(*i)->on_channel_selection_change(mask);
	}

	_last_channel_selection = mask;
}

void 
MidiRegionView::midi_patch_settings_changed(std::string model, std::string custom_device_mode)
{
	_model_name         = model;
	_custom_device_mode = custom_device_mode;
	redisplay_model();
}

void
MidiRegionView::cut_copy_clear (Editing::CutCopyOp op)
{
	if (_selection.empty()) {
		return;
	}

	PublicEditor& editor (trackview.editor());

	switch (op) {
	case Cut:
	case Copy:
		editor.get_cut_buffer().add (selection_as_cut_buffer());
		break;
	default:
		break;
	}
		
	start_delta_command();

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		switch (op) {
		case Copy:
			break;
		case Cut:
			delta_remove_note (*i);
			break;
		case Clear:
			break;
		}
	}

	apply_delta();
}

MidiCutBuffer*
MidiRegionView::selection_as_cut_buffer () const
{
	NoteList notes;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		notes.push_back (boost::shared_ptr<NoteType> (new NoteType (*((*i)->note().get()))));
	}

	/* sort them into time order */

	Evoral::Sequence<Evoral::MusicalTime>::LaterNoteComparator cmp;
	sort (notes.begin(), notes.end(),  cmp);

	MidiCutBuffer* cb = new MidiCutBuffer (trackview.session());
	cb->set (notes);
	
	return cb;
}

void
MidiRegionView::paste (nframes64_t pos, float times, const MidiCutBuffer& mcb)
{
	if (mcb.empty()) {
		return;
	}

	start_delta_command (_("paste"));

	Evoral::MusicalTime beat_delta;
	Evoral::MusicalTime paste_pos_beats;
	Evoral::MusicalTime duration;
	Evoral::MusicalTime end_point;

	duration = mcb.notes().back()->end_time() - mcb.notes().front()->time();
	paste_pos_beats = frames_to_beats (pos - _region->position());
	beat_delta = mcb.notes().front()->time() - paste_pos_beats;
	paste_pos_beats = 0;

	_selection.clear ();

	for (int n = 0; n < (int) times; ++n) {

		for (NoteList::const_iterator i = mcb.notes().begin(); i != mcb.notes().end(); ++i) {
			
			boost::shared_ptr<NoteType> copied_note (new NoteType (*((*i).get())));
			copied_note->set_time (paste_pos_beats + copied_note->time() - beat_delta);

			/* make all newly added notes selected */

			delta_add_note (copied_note, true);
			end_point = copied_note->end_time();
		}

		paste_pos_beats += duration;
	}

	/* if we pasted past the current end of the region, extend the region */

	nframes64_t end_frame = _region->position() + beats_to_frames (end_point);
	nframes64_t region_end = _region->position() + _region->length() - 1;

	if (end_frame > region_end) {

		trackview.session().begin_reversible_command (_("paste"));

		XMLNode& before (_region->get_state());
		_region->set_length (end_frame, this);
		trackview.session().add_command (new MementoCommand<Region>(*_region, &before, &_region->get_state()));
	}
	
	apply_delta ();
}

struct EventNoteTimeEarlyFirstComparator {
    bool operator() (CanvasNoteEvent* a, CanvasNoteEvent* b) {
	    return a->note()->time() < b->note()->time();
    }
};

void
MidiRegionView::time_sort_events ()
{
	if (!_sort_needed) {
		return;
	}

	EventNoteTimeEarlyFirstComparator cmp;
	_events.sort (cmp);

	_sort_needed = false;
}

void
MidiRegionView::goto_next_note ()
{
	// nframes64_t pos = -1;
	bool use_next = false;

	if (_events.back()->selected()) {
		return;
	}

	time_sort_events ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((*i)->selected()) {
			use_next = true;
			continue;
		} else if (use_next) {
			unique_select (*i);
			// pos = _region->position() + beats_to_frames ((*i)->note()->time());
			return;
		}
	}

	/* use the first one */

	unique_select (_events.front());
	
}

void
MidiRegionView::goto_previous_note ()
{
	// nframes64_t pos = -1;
	bool use_next = false;

	if (_events.front()->selected()) {
		return;
	}

	time_sort_events ();

	for (Events::reverse_iterator i = _events.rbegin(); i != _events.rend(); ++i) {
		if ((*i)->selected()) {
			use_next = true;
			continue;
		} else if (use_next) {
			unique_select (*i);
			// pos = _region->position() + beats_to_frames ((*i)->note()->time());
			return;
		}
	}

	/* use the last one */

	unique_select (*(_events.rbegin()));
}

void
MidiRegionView::selection_as_notelist (NoteList& selected) 
{
	time_sort_events ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((*i)->selected()) {
			selected.push_back ((*i)->note());
		}
	}
}


