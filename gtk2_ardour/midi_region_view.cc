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
#include "pbd/stateful_diff_command.h"

#include "ardour/playlist.h"
#include "ardour/tempo.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_model.h"
#include "ardour/midi_patch_manager.h"
#include "ardour/session.h"

#include "evoral/Parameter.hpp"
#include "evoral/MIDIParameters.hpp"
#include "evoral/Control.hpp"
#include "evoral/midi_util.h"

#include "automation_region_view.h"
#include "automation_time_axis.h"
#include "canvas-hit.h"
#include "canvas-note.h"
#include "canvas-program-change.h"
#include "editor.h"
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
#include "note_player.h"
#include "public_editor.h"
#include "rgb_macros.h"
#include "selection.h"
#include "simpleline.h"
#include "streamview.h"
#include "utils.h"
#include "mouse_cursors.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;
using Gtkmm2ext::Keyboard;

MidiRegionView::MidiRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv,
		boost::shared_ptr<MidiRegion> r, double spu, Gdk::Color const & basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, _force_channel(-1)
	, _last_channel_selection(0xFFFF)
	, _current_range_min(0)
	, _current_range_max(0)
	, _model_name(string())
	, _custom_device_mode(string())
	, _active_notes(0)
	, _note_group(new ArdourCanvas::Group(*group))
	, _note_diff_command (0)
	, _ghost_note(0)
        , _drag_rect (0)
        , _step_edit_cursor (0)
        , _step_edit_cursor_width (1.0)
        , _step_edit_cursor_position (0.0)
	, _temporary_note_group (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, no_sound_notes (false)
	, _last_event_x (0)
	, _last_event_y (0)
        , pre_enter_cursor (0)
{
	_note_group->raise_to_top();
        PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

	connect_to_diskstream ();
}

MidiRegionView::MidiRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv,
		boost::shared_ptr<MidiRegion> r, double spu, Gdk::Color& basic_color,
		TimeAxisViewItem::Visibility visibility)
	: RegionView (parent, tv, r, spu, basic_color, false, visibility)
	, _force_channel(-1)
	, _last_channel_selection(0xFFFF)
	, _model_name(string())
	, _custom_device_mode(string())
	, _active_notes(0)
	, _note_group(new ArdourCanvas::Group(*parent))
	, _note_diff_command (0)
	, _ghost_note(0)
        , _drag_rect (0)
        , _step_edit_cursor (0)
        , _step_edit_cursor_width (1.0)
        , _step_edit_cursor_position (0.0)
	, _temporary_note_group (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, no_sound_notes (false)
	, _last_event_x (0)
	, _last_event_y (0)
{
	_note_group->raise_to_top();
        PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

	connect_to_diskstream ();
}

MidiRegionView::MidiRegionView (const MidiRegionView& other)
	: sigc::trackable(other)
	, RegionView (other)
	, _force_channel(-1)
	, _last_channel_selection(0xFFFF)
	, _model_name(string())
	, _custom_device_mode(string())
	, _active_notes(0)
	, _note_group(new ArdourCanvas::Group(*get_canvas_group()))
	, _note_diff_command (0)
	, _ghost_note(0)
        , _drag_rect (0)
        , _step_edit_cursor (0)
        , _step_edit_cursor_width (1.0)
        , _step_edit_cursor_position (0.0)
	, _temporary_note_group (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, no_sound_notes (false)
	, _last_event_x (0)
	, _last_event_y (0)
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
	, _model_name(string())
	, _custom_device_mode(string())
	, _active_notes(0)
	, _note_group(new ArdourCanvas::Group(*get_canvas_group()))
	, _note_diff_command (0)
	, _ghost_note(0)
        , _drag_rect (0)
        , _step_edit_cursor (0)
        , _step_edit_cursor_width (1.0)
        , _step_edit_cursor_position (0.0)
	, _temporary_note_group (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, no_sound_notes (false)
	, _last_event_x (0)
	, _last_event_y (0)
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
        PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

        CanvasNoteEvent::CanvasNoteEventDeleted.connect (note_delete_connection, MISSING_INVALIDATOR, 
                                                         ui_bind (&MidiRegionView::maybe_remove_deleted_note_from_selection, this, _1),
                                                         gui_context());

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
	region_resized (ARDOUR::bounds_change);
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
	group->signal_event().connect (sigc::mem_fun (this, &MidiRegionView::canvas_event), false);

	midi_view()->signal_channel_mode_changed().connect(
			sigc::mem_fun(this, &MidiRegionView::midi_channel_mode_changed));

	midi_view()->signal_midi_patch_settings_changed().connect(
			sigc::mem_fun(this, &MidiRegionView::midi_patch_settings_changed));

	trackview.editor().SnapChanged.connect (snap_changed_connection, invalidator (*this), ui_bind (&MidiRegionView::snap_changed, this), gui_context ());

	connect_to_diskstream ();
}

void
MidiRegionView::connect_to_diskstream ()
{
	midi_view()->midi_track()->DataRecorded.connect (*this, invalidator (*this), ui_bind (&MidiRegionView::data_recorded, this, _1, _2), gui_context ());
}

bool
MidiRegionView::canvas_event(GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		_last_event_x = ev->crossing.x;
		_last_event_y = ev->crossing.y;
		break;
	case GDK_MOTION_NOTIFY:
		_last_event_x = ev->motion.x;
		_last_event_y = ev->motion.y;
		break;
	default:
		break;
	}
	
	if (!trackview.editor().internal_editing()) {
		return false;
	}

	/* XXX: note that until version 2.30, the GnomeCanvas did not propagate scroll events
	   to its items, which means that ev->type == GDK_SCROLL will never be seen
	*/

	switch (ev->type) {
	case GDK_SCROLL:
                return scroll (&ev->scroll);

	case GDK_KEY_PRESS:
                return key_press (&ev->key);

	case GDK_KEY_RELEASE:
                return key_release (&ev->key);

	case GDK_BUTTON_PRESS:
                return button_press (&ev->button);

	case GDK_2BUTTON_PRESS:
		return true;

	case GDK_BUTTON_RELEASE:
                return button_release (&ev->button);
		
	case GDK_ENTER_NOTIFY:
                return enter_notify (&ev->crossing);

	case GDK_LEAVE_NOTIFY:
                return leave_notify (&ev->crossing);

	case GDK_MOTION_NOTIFY:
                return motion (&ev->motion);

	default: 
                break;
	}

	return false;
}

void
MidiRegionView::remove_ghost_note ()
{
        delete _ghost_note;
        _ghost_note = 0;
}

bool
MidiRegionView::enter_notify (GdkEventCrossing* ev)
{
	trackview.editor().MouseModeChanged.connect (
		_mouse_mode_connection, invalidator (*this), ui_bind (&MidiRegionView::mouse_mode_changed, this), gui_context ()
		);

        Keyboard::magic_widget_grab_focus();
        group->grab_focus();

	if (trackview.editor().current_mouse_mode() == MouseRange) {
		create_ghost_note (ev->x, ev->y);
	}

        return false;
}

bool
MidiRegionView::leave_notify (GdkEventCrossing*)
{
	_mouse_mode_connection.disconnect ();
	
        trackview.editor().hide_verbose_canvas_cursor ();
	remove_ghost_note ();
        return false;
}

void
MidiRegionView::mouse_mode_changed ()
{
	if (trackview.editor().current_mouse_mode() == MouseRange && trackview.editor().internal_editing()) {
		create_ghost_note (_last_event_x, _last_event_y);
	} else {
		remove_ghost_note ();
		trackview.editor().hide_verbose_canvas_cursor ();
	}
}

bool
MidiRegionView::button_press (GdkEventButton* ev)
{
        _last_x = ev->x;
        _last_y = ev->y;
        group->w2i (_last_x, _last_y);
        
        if (_mouse_state != SelectTouchDragging && ev->button == 1) {
                _pressed_button = ev->button;
                _mouse_state = Pressed;
                return true;
        }
        
        _pressed_button = ev->button;

        return true;
}

bool
MidiRegionView::button_release (GdkEventButton* ev)
{
	double event_x, event_y;
	framepos_t event_frame = 0;

        event_x = ev->x;
        event_y = ev->y;
        group->w2i(event_x, event_y);
        group->ungrab(ev->time);
        event_frame = trackview.editor().pixel_to_frame(event_x);

        if (ev->button == 3) {
                return false;
        } else if (_pressed_button != 1) {
                return false;
        }

        switch (_mouse_state) {
        case Pressed: // Clicked
                switch (trackview.editor().current_mouse_mode()) {
                case MouseObject:
                case MouseTimeFX:
                        clear_selection();
                        maybe_select_by_position (ev, event_x, event_y);
                        break;

                case MouseRange:
                {
                        bool success;
                        Evoral::MusicalTime beats = trackview.editor().get_grid_type_as_beats (success, trackview.editor().pixel_to_frame (event_x));
                        if (!success) {
                                beats = 1;
                        }
                        create_note_at (event_x, event_y, beats, true);
                        break;
                }
                default:
                        break;
                }
                _mouse_state = None;
                break;
        case SelectRectDragging: // Select drag done
                _mouse_state = None;
                delete _drag_rect;
                _drag_rect = 0;
                break;

        case AddDragging: // Add drag done
                _mouse_state = None;
                if (_drag_rect->property_x2() > _drag_rect->property_x1() + 2) {
                        const double x      = _drag_rect->property_x1();
                        const double length = trackview.editor().pixel_to_frame 
                                (_drag_rect->property_x2() - _drag_rect->property_x1());

                        create_note_at (x, _drag_rect->property_y1(), frames_to_beats(length), true);
                }

                delete _drag_rect;
                _drag_rect = 0;

                create_ghost_note (ev->x, ev->y);

        default:
                break;
        }

        return false;
}

bool
MidiRegionView::motion (GdkEventMotion* ev)
{
	double event_x, event_y;
	framepos_t event_frame = 0;

        event_x = ev->x;
        event_y = ev->y;
        group->w2i(event_x, event_y);

        // convert event_x to global frame
        event_frame = trackview.editor().pixel_to_frame(event_x) + _region->position();
        trackview.editor().snap_to(event_frame);
        // convert event_frame back to local coordinates relative to position
        event_frame -= _region->position();

        if (_ghost_note) {
                update_ghost_note (ev->x, ev->y);
        }

        /* any motion immediately hides velocity text that may have been visible */
		
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->hide_velocity ();
	}

        switch (_mouse_state) {
        case Pressed: // Maybe start a drag, if we've moved a bit

                if (fabs (event_x - _last_x) < 1 && fabs (event_y - _last_y) < 1) {
                        /* no appreciable movement since the button was pressed */
                        return false;
                }

                // Select drag start
                if (_pressed_button == 1 && trackview.editor().current_mouse_mode() == MouseObject) {
                        group->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                                    Gdk::Cursor(Gdk::FLEUR), ev->time);
                        _last_x = event_x;
                        _last_y = event_y;
                        _drag_start_x = event_x;
                        _drag_start_y = event_y;

                        _drag_rect = new ArdourCanvas::SimpleRect(*group);
                        _drag_rect->property_x1() = event_x;
                        _drag_rect->property_y1() = event_y;
                        _drag_rect->property_x2() = event_x;
                        _drag_rect->property_y2() = event_y;
                        _drag_rect->property_outline_what() = 0xFF;
                        _drag_rect->property_outline_color_rgba()
                                = ARDOUR_UI::config()->canvasvar_MidiSelectRectOutline.get();
                        _drag_rect->property_fill_color_rgba()
                                = ARDOUR_UI::config()->canvasvar_MidiSelectRectFill.get();

                        _mouse_state = SelectRectDragging;
                        return true;

			// Add note drag start
                } else if (trackview.editor().internal_editing()) {

                        delete _ghost_note;
                        _ghost_note = 0;
				
                        group->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                                    Gdk::Cursor(Gdk::FLEUR), ev->time);
                        _last_x = event_x;
                        _last_y = event_y;
                        _drag_start_x = event_x;
                        _drag_start_y = event_y;

                        _drag_rect = new ArdourCanvas::SimpleRect(*group);
                        _drag_rect->property_x1() = trackview.editor().frame_to_pixel(event_frame);

                        _drag_rect->property_y1() = midi_stream_view()->note_to_y(
                                midi_stream_view()->y_to_note(event_y));
                        _drag_rect->property_x2() = trackview.editor().frame_to_pixel(event_frame);
                        _drag_rect->property_y2() = _drag_rect->property_y1()
                                + floor(midi_stream_view()->note_height());
                        _drag_rect->property_outline_what() = 0xFF;
                        _drag_rect->property_outline_color_rgba() = 0xFFFFFF99;
                        _drag_rect->property_fill_color_rgba()    = 0xFFFFFF66;

                        _mouse_state = AddDragging;
                        return true;
                }

                return false;

        case SelectRectDragging: // Select drag motion
        case AddDragging: // Add note drag motion
                if (ev->is_hint) {
                        int t_x;
                        int t_y;
                        GdkModifierType state;
                        gdk_window_get_pointer(ev->window, &t_x, &t_y, &state);
                        event_x = t_x;
                        event_y = t_y;
                }

                if (_mouse_state == AddDragging)
                        event_x = trackview.editor().frame_to_pixel(event_frame);

                if (_drag_rect) {
                        if (event_x > _drag_start_x)
                                _drag_rect->property_x2() = event_x;
                        else
                                _drag_rect->property_x1() = event_x;
                }

                if (_drag_rect && _mouse_state == SelectRectDragging) {
                        if (event_y > _drag_start_y)
                                _drag_rect->property_y2() = event_y;
                        else
                                _drag_rect->property_y1() = event_y;

                        update_drag_selection(_drag_start_x, event_x, _drag_start_y, event_y);
                }

                _last_x = event_x;
                _last_y = event_y;

        case SelectTouchDragging:
                return false;

        default:
                break;
        }

        return false;
}


bool
MidiRegionView::scroll (GdkEventScroll* ev)
{
        if (_selection.empty()) {
                return false;
        }

	trackview.editor().hide_verbose_canvas_cursor ();

        bool fine = !Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier);
        
        if (ev->direction == GDK_SCROLL_UP) {
                change_velocities (true, fine, false);
        } else if (ev->direction == GDK_SCROLL_DOWN) {
                change_velocities (false, fine, false);
        } 
        return true;
}

bool
MidiRegionView::key_press (GdkEventKey* ev)
{ 
        /* since GTK bindings are generally activated on press, and since
           detectable auto-repeat is the name of the game and only sends
           repeated presses, carry out key actions at key press, not release.
        */
        
        if (ev->keyval == GDK_Alt_L || ev->keyval == GDK_Alt_R){
                _mouse_state = SelectTouchDragging;
                return true;
                
        } else if (ev->keyval == GDK_Escape) {
                clear_selection();
                _mouse_state = None;
                
        } else if (ev->keyval == GDK_comma || ev->keyval == GDK_period) {
                
                bool start = (ev->keyval == GDK_comma);
                bool end = (ev->keyval == GDK_period);
                bool shorter = Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier);
                bool fine = Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
                
                change_note_lengths (fine, shorter, 0.0, start, end);
                
                return true;
                
        } else if (ev->keyval == GDK_Delete) {
                
                delete_selection();
                return true;
                
        } else if (ev->keyval == GDK_Tab) {
                
                if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
                        goto_previous_note ();
                } else {
                        goto_next_note ();
                }
                return true;
                
        } else if (ev->keyval == GDK_Up) {
                
                bool allow_smush = Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier);
                bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
                
                if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
                        change_velocities (true, fine, allow_smush);
                } else {
                        transpose (true, fine, allow_smush);
                }
                return true;
                
        } else if (ev->keyval == GDK_Down) {
                
                bool allow_smush = Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier);
                bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
                
                if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
                        change_velocities (false, fine, allow_smush);
                } else {
                        transpose (false, fine, allow_smush);
                }
                return true;
                
        } else if (ev->keyval == GDK_Left) {
                
                nudge_notes (false);
                return true;
                
        } else if (ev->keyval == GDK_Right) {
                
                nudge_notes (true);
                return true;
                
        } else if (ev->keyval == GDK_Control_L) {
                return true;

        }
        
        return false;
}

bool
MidiRegionView::key_release (GdkEventKey* ev)
{
        if (ev->keyval == GDK_Alt_L || ev->keyval == GDK_Alt_R) {
                _mouse_state = None;
                return true;
        }
        return false;
}

void
MidiRegionView::show_list_editor ()
{
	if (!_list_editor) {
		_list_editor = new MidiListEditor (trackview.session(), midi_region());
	}
	_list_editor->present ();
}

/** Add a note to the model, and the view, at a canvas (click) coordinate.
 * \param x horizontal position in pixels
 * \param y vertical position in pixels
 * \param length duration of the note in beats, which will be snapped to the grid
 * \param sh true to make the note 1 frame shorter than the snapped version of \a length.
 */
void
MidiRegionView::create_note_at(double x, double y, double length, bool sh)
{
	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	MidiStreamView* const view = mtv->midi_view();

	double note = midi_stream_view()->y_to_note(y);

	assert(note >= 0.0);
	assert(note <= 127.0);

	// Start of note in frames relative to region start
	framepos_t const start_frames = snap_frame_to_frame(trackview.editor().pixel_to_frame(x));
	assert(start_frames >= 0);

	// Snap length
	length = frames_to_beats(
			snap_frame_to_frame(start_frames + beats_to_frames(length)) - start_frames);

	assert (length != 0);

	if (sh) {
		length = frames_to_beats (beats_to_frames (length) - 1);
	}

	uint16_t chn_mask = mtv->channel_selector().get_selected_channels();
        int chn_cnt = 0;
        uint8_t channel = 0;

        /* pick the highest selected channel, unless all channels are selected,
           which is interpreted to mean channel 1 (zero)
        */

        for (uint16_t i = 0; i < 16; ++i) {
                if (chn_mask & (1<<i)) {
                        channel = i;
                        chn_cnt++;
                }
        }

        if (chn_cnt == 16) {
                channel = 0;
        }

	const boost::shared_ptr<NoteType> new_note (new NoteType (channel,
                                                                  frames_to_beats(start_frames + _region->start()), length,
                                                                  (uint8_t)note, 0x40));

        if (_model->contains (new_note)) {
                return;
        }

	view->update_note_range(new_note->note());

	MidiModel::NoteDiffCommand* cmd = _model->new_note_diff_command("add note");
	cmd->add (new_note);
	_model->apply_command(*trackview.session(), cmd);

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
	_model->ContentsChanged.connect (content_connection, invalidator (*this), boost::bind (&MidiRegionView::redisplay_model, this), gui_context());

	clear_events ();

	if (_enable_display) {
		redisplay_model();
	}
}

void
MidiRegionView::start_note_diff_command (string name)
{
	if (!_note_diff_command) {
		_note_diff_command = _model->new_note_diff_command (name);
	}
}

void
MidiRegionView::note_diff_add_note (const boost::shared_ptr<NoteType> note, bool selected, bool show_velocity)
{
	if (_note_diff_command) {
		_note_diff_command->add (note);
	}
	if (selected) {
		_marked_for_selection.insert(note);
	}
	if (show_velocity) {
		_marked_for_velocity.insert(note);
	}
}

void
MidiRegionView::note_diff_remove_note (ArdourCanvas::CanvasNoteEvent* ev)
{
	if (_note_diff_command && ev->note()) {
		_note_diff_command->remove(ev->note());
	}
}

void
MidiRegionView::note_diff_add_change (ArdourCanvas::CanvasNoteEvent* ev,
				      MidiModel::NoteDiffCommand::Property property,
				      uint8_t val)
{
	if (_note_diff_command) {
		_note_diff_command->change (ev->note(), property, val);
	}
}

void
MidiRegionView::note_diff_add_change (ArdourCanvas::CanvasNoteEvent* ev,
				      MidiModel::NoteDiffCommand::Property property,
				      Evoral::MusicalTime val)
{
	if (_note_diff_command) {
		_note_diff_command->change (ev->note(), property, val);
	}
}

void
MidiRegionView::apply_diff ()
{
        bool add_or_remove;

	if (!_note_diff_command) {
		return;
	}

        if ((add_or_remove = _note_diff_command->adds_or_removes())) {
                // Mark all selected notes for selection when model reloads
                for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
                        _marked_for_selection.insert((*i)->note());
                }
        }

	_model->apply_command(*trackview.session(), _note_diff_command);
	_note_diff_command = 0;
	midi_view()->midi_track()->playlist_modified();
        
        if (add_or_remove) {
        	_marked_for_selection.clear();
        }

	_marked_for_velocity.clear();
}

void
MidiRegionView::apply_diff_as_subcommand ()
{
        bool add_or_remove;

	if (!_note_diff_command) {
		return;
	}

        if ((add_or_remove = _note_diff_command->adds_or_removes())) {
                // Mark all selected notes for selection when model reloads
                for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
                        _marked_for_selection.insert((*i)->note());
                }
        }

	_model->apply_command_as_subcommand(*trackview.session(), _note_diff_command);
	_note_diff_command = 0;
	midi_view()->midi_track()->playlist_modified();

        if (add_or_remove) {
                _marked_for_selection.clear();
        }
	_marked_for_velocity.clear();
}


void
MidiRegionView::abort_command()
{
	delete _note_diff_command;
	_note_diff_command = 0;
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
MidiRegionView::get_events (Events& e, Evoral::Sequence<Evoral::MusicalTime>::NoteOperator op, uint8_t val, int chan_mask)
{
        MidiModel::Notes notes;
        _model->get_notes (notes, op, val, chan_mask);

        for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {
                CanvasNoteEvent* cne = find_canvas_note (*n);
                if (cne) {
                        e.push_back (cne);
                }
        }
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

	MidiModel::ReadLock lock(_model->read_lock());

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

	_pgm_changes.clear();
	_sys_exes.clear();
	
	display_sysexes();
	display_program_changes();

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
	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	uint16_t chn_mask = mtv->channel_selector().get_selected_channels();

        for (uint8_t i = 0; i < 16; ++i) {
                if (chn_mask & (1<<i)) {
                        display_program_changes_on_channel (i);
                }
        }
}

void
MidiRegionView::display_program_changes_on_channel(uint8_t channel)
{
	boost::shared_ptr<Evoral::Control> control = 
                _model->control(Evoral::MIDI::ProgramChange (MidiPgmChangeAutomation, channel));

	if (!control) {
		return;
	}

	Glib::Mutex::Lock lock (control->list()->lock());

	for (AutomationList::const_iterator event = control->list()->begin();
			event != control->list()->end(); ++event) {
		double event_time     = (*event)->when;
		double program_number = floor((*event)->value + 0.5);

		// Get current value of bank select MSB at time of the program change
		Evoral::Parameter bank_select_msb(MidiCCAutomation, channel, MIDI_CTL_MSB_BANK);
		boost::shared_ptr<Evoral::Control> msb_control = _model->control(bank_select_msb);
		uint8_t msb = 0;
		if (msb_control != 0) {
			msb = uint8_t(floor(msb_control->get_double(true, event_time) + 0.5));
		}

		// Get current value of bank select LSB at time of the program change
		Evoral::Parameter bank_select_lsb(MidiCCAutomation, channel, MIDI_CTL_LSB_BANK);
		boost::shared_ptr<Evoral::Control> lsb_control = _model->control(bank_select_lsb);
		uint8_t lsb = 0;
		if (lsb_control != 0) {
			lsb = uint8_t(floor(lsb_control->get_double(true, event_time) + 0.5));
		}

		MIDI::Name::PatchPrimaryKey patch_key(msb, lsb, program_number);

		boost::shared_ptr<MIDI::Name::Patch> patch =
			MIDI::Name::MidiPatchManager::instance().find_patch(
					_model_name, _custom_device_mode, channel, patch_key);

		PCEvent program_change(event_time, uint8_t(program_number), channel);

		if (patch != 0) {
			add_canvas_program_change (program_change, patch->name());
		} else {
			char buf[4];
                        // program_number is zero-based: convert to one-based
			snprintf(buf, 4, "%d", int(program_number+1));
			add_canvas_program_change (program_change, buf);
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

		const double x = trackview.editor().frame_to_pixel(beats_to_frames(time));

		double height = midi_stream_view()->contents_height();

		boost::shared_ptr<CanvasSysEx> sysex = boost::shared_ptr<CanvasSysEx>(
				new CanvasSysEx(*this, *_note_group, text, height, x, 1.0));

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

	trackview.editor().hide_verbose_canvas_cursor ();

        note_delete_connection.disconnect ();

	delete _list_editor;

	RegionViewGoingAway (this); /* EMIT_SIGNAL */

	if (_active_notes) {
		end_write();
	}

	_selection.clear();
	clear_events();

	delete _note_group;
	delete _note_diff_command;
        delete _step_edit_cursor;
	delete _temporary_note_group;
}

void
MidiRegionView::region_resized (const PropertyChange& what_changed)
{
	RegionView::region_resized(what_changed);

	if (what_changed.contains (ARDOUR::Properties::position)) {
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
        
        move_step_edit_cursor (_step_edit_cursor_position);
        set_step_edit_cursor_width (_step_edit_cursor_width);
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
        
        for (PgmChanges::iterator x = _pgm_changes.begin(); x != _pgm_changes.end(); ++x) {
                (*x)->set_height (midi_stream_view()->contents_height());
        }

        if (_step_edit_cursor) {
                _step_edit_cursor->property_y2() = midi_stream_view()->contents_height();
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

			const double diamond_size = update_hit (chit);

			chit->set_height (diamond_size);
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

	GhostRegion::CatchDeletion.connect (*this, invalidator (*this), ui_bind (&RegionView::remove_ghost, this, _1), gui_context());

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
		const framepos_t end_time_frames = beats_to_frames(end_time) - _region->start();
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
	if (no_sound_notes || !trackview.editor().sound_notes()) {
		return;
	}

	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);
        
        if (!route_ui || !route_ui->midi_track()) {
                return;
        }

        NotePlayer* np = new NotePlayer (route_ui->midi_track());
        np->add (note);
        np->play ();
}

void
MidiRegionView::play_midi_chord (vector<boost::shared_ptr<NoteType> > notes)
{
	if (no_sound_notes || !trackview.editor().sound_notes()) {
		return;
	}

	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);
        
        if (!route_ui || !route_ui->midi_track()) {
                return;
        }

        NotePlayer* np = new NotePlayer (route_ui->midi_track());

        for (vector<boost::shared_ptr<NoteType> >::iterator n = notes.begin(); n != notes.end(); ++n) {
                np->add (*n);
        }

        np->play ();
}


bool
MidiRegionView::note_in_region_range(const boost::shared_ptr<NoteType> note, bool& visible) const
{
	const framepos_t note_start_frames = beats_to_frames(note->time());

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

	const framepos_t note_start_frames = beats_to_frames(note->time());

	/* trim note display to not overlap the end of its region */
	const framepos_t note_end_frames = min (beats_to_frames (note->end_time()), _region->start() + _region->length());

	const double x = trackview.editor().frame_to_pixel(note_start_frames - _region->start());
	const double y1 = midi_stream_view()->note_to_y(note->note());
	const double note_endpixel = trackview.editor().frame_to_pixel(note_end_frames - _region->start());

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

double
MidiRegionView::update_hit (CanvasHit* ev)
{
	boost::shared_ptr<NoteType> note = ev->note();

	const framepos_t note_start_frames = beats_to_frames(note->time());
	const double x = trackview.editor().frame_to_pixel(note_start_frames - _region->start());
	const double diamond_size = midi_stream_view()->note_height() / 2.0;
	const double y = midi_stream_view()->note_to_y(note->note()) + ((diamond_size-2) / 4.0);

	ev->move_to (x, y);

	return diamond_size;
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

	if (midi_view()->note_mode() == Sustained) {

		CanvasNote* ev_rect = new CanvasNote(*this, *_note_group, note);

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

		CanvasHit* ev_diamond = new CanvasHit(*this, *_note_group, diamond_size, note);

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
MidiRegionView::step_add_note (uint8_t channel, uint8_t number, uint8_t velocity,
                               Evoral::MusicalTime pos, Evoral::MusicalTime len)
{
	boost::shared_ptr<NoteType> new_note (new NoteType (channel, pos, len, number, velocity));

	/* potentially extend region to hold new note */

	framepos_t end_frame = _region->position() + beats_to_frames (new_note->end_time());
	framepos_t region_end = _region->position() + _region->length() - 1;

	if (end_frame > region_end) {
		_region->set_length (end_frame - _region->position(), this);
	}

        _marked_for_selection.clear ();
        clear_selection ();

	start_note_diff_command (_("step add"));
	note_diff_add_note (new_note, true, false);
	apply_diff();

        // last_step_edit_note = new_note;
}

void
MidiRegionView::step_sustain (Evoral::MusicalTime beats)
{
        change_note_lengths (false, false, beats, false, true);
}

void
MidiRegionView::add_canvas_program_change (PCEvent& program, const string& displaytext)
{
	assert(program.time >= 0);

	const double x = trackview.editor().frame_to_pixel(beats_to_frames(program.time));

	double height = midi_stream_view()->contents_height();

	boost::shared_ptr<CanvasProgramChange> pgm_change = boost::shared_ptr<CanvasProgramChange>(
			new CanvasProgramChange(*this, *_note_group,
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
	Evoral::Parameter bank_select_msb(MidiCCAutomation, channel, MIDI_CTL_MSB_BANK);
	boost::shared_ptr<Evoral::Control> msb_control = _model->control(bank_select_msb);
	double msb = 0.0;
	if (msb_control != 0) {
		msb = int(msb_control->get_double(true, time));
	}

	Evoral::Parameter bank_select_lsb(MidiCCAutomation, channel, MIDI_CTL_LSB_BANK);
	boost::shared_ptr<Evoral::Control> lsb_control = _model->control(bank_select_lsb);
	double lsb = 0.0;
	if (lsb_control != 0) {
		lsb = lsb_control->get_double(true, time);
	}

	Evoral::Parameter program_change(MidiPgmChangeAutomation, channel, 0);
	boost::shared_ptr<Evoral::Control> program_control = _model->control(program_change);
	double program_number = -1.0;
	if (program_control != 0) {
		program_number = program_control->get_double(true, time);
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
		msb_control->set_double(double(new_patch.msb), true, old_program.time);
	}

	// TODO: Get the real event here and alter them at the original times
	Evoral::Parameter bank_select_lsb(MidiCCAutomation, old_program.channel, MIDI_CTL_LSB_BANK);
	boost::shared_ptr<Evoral::Control> lsb_control = _model->control(bank_select_lsb);
	if (lsb_control != 0) {
		lsb_control->set_double(double(new_patch.lsb), true, old_program.time);
	}

	Evoral::Parameter program_change(MidiPgmChangeAutomation, old_program.channel, 0);
	boost::shared_ptr<Evoral::Control> program_control = _model->control(program_change);

	assert(program_control != 0);
	program_control->set_double(float(new_patch.program_number), true, old_program.time);

        _pgm_changes.clear ();
        display_program_changes (); // XXX would be nice to limit to just old_program.channel
}

/** @param t Time in frames relative to region position */
void
MidiRegionView::add_program_change (framecnt_t t, uint8_t channel, uint8_t value)
{
	boost::shared_ptr<Evoral::Control> control = midi_region()->model()->control (
		Evoral::Parameter (MidiPgmChangeAutomation, channel, 0), true
		);
	
	assert (control);

	Evoral::MusicalTime const b = frames_to_beats (t + midi_region()->start());

	control->list()->add (b, value);

	_pgm_changes.clear ();
	display_program_changes ();
}

void
MidiRegionView::move_program_change (PCEvent pc, Evoral::MusicalTime t)
{
	boost::shared_ptr<Evoral::Control> control = _model->control (Evoral::Parameter (MidiPgmChangeAutomation, pc.channel, 0));
	assert (control);

	control->list()->erase (pc.time, pc.value);
	control->list()->add (t, pc.value);

	_pgm_changes.clear ();
	display_program_changes ();
}

void
MidiRegionView::delete_program_change (CanvasProgramChange* pc)
{
	boost::shared_ptr<Evoral::Control> control = _model->control (Evoral::Parameter (MidiPgmChangeAutomation, pc->channel(), 0));
	assert (control);

	control->list()->erase (pc->event_time(), pc->program());
	_pgm_changes.clear ();
	display_program_changes ();
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
        if (program.program() < 127) {
                MIDI::Name::PatchPrimaryKey key;
                get_patch_key_at(program.event_time(), program.channel(), key);
                PCEvent program_change_event(program.event_time(), program.program(), program.channel());

                key.program_number++;
                alter_program_change(program_change_event, key);
        }
}

void
MidiRegionView::next_program(CanvasProgramChange& program)
{
        if (program.program() > 0) {
                MIDI::Name::PatchPrimaryKey key;
                get_patch_key_at(program.event_time(), program.channel(), key);
                PCEvent program_change_event(program.event_time(), program.program(), program.channel());

                key.program_number--;
                alter_program_change(program_change_event, key);
        }
}

void
MidiRegionView::maybe_remove_deleted_note_from_selection (CanvasNoteEvent* cne)
{
        if (_selection.empty()) {
                return;
        }
 
        if (_selection.erase (cne) > 0) {
                cerr << "Erased a CNE from selection\n";
        }
}

void
MidiRegionView::delete_selection()
{
	if (_selection.empty()) {
		return;
	}

	start_note_diff_command (_("delete selection"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->selected()) {
			_note_diff_command->remove((*i)->note());
		}
	}

	_selection.clear();

	apply_diff ();
}

void
MidiRegionView::delete_note (boost::shared_ptr<NoteType> n)
{
	start_note_diff_command (_("delete note"));
	_note_diff_command->remove (n);
	apply_diff ();

	trackview.editor().hide_verbose_canvas_cursor ();
}

void
MidiRegionView::clear_selection_except(ArdourCanvas::CanvasNoteEvent* ev)
{
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->selected() && (*i) != ev) {
			(*i)->set_selected(false);
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

			(*i)->set_selected (false);
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
MidiRegionView::select_matching_notes (uint8_t notenum, uint16_t channel_mask, bool add, bool extend)
{
	uint8_t low_note = 127;
	uint8_t high_note = 0;
	MidiModel::Notes& notes (_model->notes());
	_optimization_iterator = _events.begin();

        if (!add) {
                clear_selection ();
        }

	if (extend && _selection.empty()) {
		extend = false;
	}

	if (extend) {

		/* scan existing selection to get note range */

		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if ((*i)->note()->note() < low_note) {
				low_note = (*i)->note()->note();
			}
			if ((*i)->note()->note() > high_note) {
				high_note = (*i)->note()->note();
			}
		}

		low_note = min (low_note, notenum);
		high_note = max (high_note, notenum);
	}

	no_sound_notes = true;

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		boost::shared_ptr<NoteType> note (*n);
		CanvasNoteEvent* cne;
		bool select = false;

		if (((1 << note->channel()) & channel_mask) != 0) {
			if (extend) {
				if ((note->note() >= low_note && note->note() <= high_note)) {
					select = true;
				}
			} else if (note->note() == notenum) {
				select = true;
			}
		}

		if (select) {
			if ((cne = find_canvas_note (note)) != 0) {
				// extend is false because we've taken care of it, 
				// since it extends by time range, not pitch.
				note_selected (cne, add, false);
			}
		}
		
		add = true; // we need to add all remaining matching notes, even if the passed in value was false (for "set")

	}

	no_sound_notes = false;
}

void
MidiRegionView::toggle_matching_notes (uint8_t notenum, uint16_t channel_mask)
{
	MidiModel::Notes& notes (_model->notes());
	_optimization_iterator = _events.begin();

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		boost::shared_ptr<NoteType> note (*n);
		CanvasNoteEvent* cne;

		if (note->note() == notenum && (((0x0001 << note->channel()) & channel_mask) != 0)) {
			if ((cne = find_canvas_note (note)) != 0) {
				if (cne->selected()) {
					note_deselected (cne);
				} else {
					note_selected (cne, true, false);
				}
			}
		}
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

		Evoral::MusicalTime earliest = Evoral::MaxMusicalTime;
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

	ev->set_selected (false);
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
		ev->set_selected (true);
		play_midi_note ((ev)->note());
        }

	if (add_mrv_selection) {
		PublicEditor& editor (trackview.editor());
		editor.get_selection().add (this);
	}
}

void
MidiRegionView::move_selection(double dx, double dy, double cumulative_dy)
{
        typedef vector<boost::shared_ptr<NoteType> > PossibleChord;
        PossibleChord to_play;
        Evoral::MusicalTime earliest = Evoral::MaxMusicalTime;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
                if ((*i)->note()->time() < earliest) {
                        earliest = (*i)->note()->time();
                }
        }

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
                if (Evoral::musical_time_equal ((*i)->note()->time(), earliest)) {
                        to_play.push_back ((*i)->note());
                }
		(*i)->move_event(dx, dy);
        }

        if (dy && !_selection.empty() && !no_sound_notes && trackview.editor().sound_notes()) {

                if (to_play.size() > 1) {

                        PossibleChord shifted;

                        for (PossibleChord::iterator n = to_play.begin(); n != to_play.end(); ++n) {
                                boost::shared_ptr<NoteType> moved_note (new NoteType (**n));
                                moved_note->set_note (moved_note->note() + cumulative_dy);
                                shifted.push_back (moved_note);
                        }

                        play_midi_chord (shifted);

                } else if (!to_play.empty()) {

                        boost::shared_ptr<NoteType> moved_note (new NoteType (*to_play.front()));
                        moved_note->set_note (moved_note->note() + cumulative_dy);
                        play_midi_note (moved_note);
                }
        }
}

void
MidiRegionView::note_dropped(CanvasNoteEvent *, frameoffset_t dt, int8_t dnote)
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

	start_note_diff_command (_("move notes"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end() ; ++i) {

		Evoral::MusicalTime new_time = frames_to_beats (beats_to_frames ((*i)->note()->time()) + dt);

		if (new_time < 0) {
			continue;
		}

		note_diff_add_change (*i, MidiModel::NoteDiffCommand::StartTime, new_time);

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

		note_diff_add_change (*i, MidiModel::NoteDiffCommand::NoteNumber, new_pitch);
	}

	apply_diff();

	// care about notes being moved beyond the upper/lower bounds on the canvas
	if (lowest_note_in_selection  < midi_stream_view()->lowest_note() ||
	    highest_note_in_selection > midi_stream_view()->highest_note()) {
		midi_stream_view()->set_note_range(MidiStreamView::ContentsRange);
	}
}

framepos_t
MidiRegionView::snap_pixel_to_frame(double x)
{
	PublicEditor& editor = trackview.editor();
	// x is region relative, convert it to global absolute frames
	framepos_t frame = editor.pixel_to_frame(x) + _region->position();
	editor.snap_to(frame);
	return frame - _region->position(); // convert back to region relative
}

framepos_t
MidiRegionView::snap_frame_to_frame(framepos_t x)
{
	PublicEditor& editor = trackview.editor();
	// x is region relative, convert it to global absolute frames
	framepos_t frame = x + _region->position();
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
	framepos_t region_frame = get_position();
	return trackview.editor().frame_to_pixel(region_frame);
}

double
MidiRegionView::get_end_position_pixels()
{
	framepos_t frame = get_position() + get_duration ();
	return trackview.editor().frame_to_pixel(frame);
}

framepos_t
MidiRegionView::beats_to_frames(double beats) const
{
	return _time_converter.to(beats);
}

double
MidiRegionView::frames_to_beats(framepos_t frames) const
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
					*_note_group, note->x1(), note->y1(), note->x2(), note->y2());

			// calculate the colors: get the color settings
			uint32_t fill_color = UINT_RGBA_CHANGE_A(
					ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get(),
					128);

			// make the resize preview notes more transparent and bright
			fill_color = UINT_INTERPOLATE(fill_color, 0xFFFFFF40, 0.5);

			// calculate color based on note velocity
			resize_rect->property_fill_color_rgba() = UINT_INTERPOLATE(
                                CanvasNoteEvent::meter_style_fill_color(note->note()->velocity(), note->selected()),
					fill_color,
					0.85);

			resize_rect->property_outline_color_rgba() = CanvasNoteEvent::calculate_outline(
					ARDOUR_UI::config()->canvasvar_MidiNoteSelected.get());

			resize_data->resize_rect = resize_rect;
			_resize_data.push_back(resize_data);
		}
	}
}

/** Update resizing notes while user drags.
 * @param primary `primary' note for the drag; ie the one that is used as the reference in non-relative mode.
 * @param at_front which end of the note (true == note on, false == note off)
 * @param delta_x change in mouse position since the start of the drag 
 * @param relative true if relative resizing is taking place, false if absolute resizing.  This only makes
 * a difference when multiple notes are being resized; in relative mode, each note's length is changed by the
 * amount of the drag.  In non-relative mode, all selected notes are set to have the same start or end point
 * as the \a primary note.
 */
void
MidiRegionView::update_resizing (ArdourCanvas::CanvasNoteEvent* primary, bool at_front, double delta_x, bool relative)
{
        bool cursor_set = false;

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		SimpleRect* resize_rect = (*i)->resize_rect;
		CanvasNote* canvas_note = (*i)->canvas_note;
		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x1() + delta_x;
			} else {
				current_x = primary->x1() + delta_x;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x2() + delta_x;
			} else {
				current_x = primary->x2() + delta_x;
			}
		}

		if (at_front) {
			resize_rect->property_x1() = snap_to_pixel(current_x);
			resize_rect->property_x2() = canvas_note->x2();
		} else {
			resize_rect->property_x2() = snap_to_pixel(current_x);
			resize_rect->property_x1() = canvas_note->x1();
		}

                if (!cursor_set) {
                        double beats;

                        beats = snap_pixel_to_frame (current_x);
                        beats = frames_to_beats (beats);
                        
                        double len;

                        if (at_front) {
                                if (beats < canvas_note->note()->end_time()) {
                                        len = canvas_note->note()->time() - beats;
                                        len += canvas_note->note()->length();
                                } else {
                                        len = 0;
                                }
                        } else {
                                if (beats >= canvas_note->note()->time()) { 
                                        len = beats - canvas_note->note()->time();
                                } else {
                                        len = 0;
                                }
                        }

                        char buf[16];
                        snprintf (buf, sizeof (buf), "%.3g beats", len);
                        trackview.editor().show_verbose_canvas_cursor_with (buf);

                        cursor_set = true;
                }

	}
}


/** Finish resizing notes when the user releases the mouse button.
 *  Parameters the same as for \a update_resizing().
 */
void
MidiRegionView::commit_resizing (ArdourCanvas::CanvasNoteEvent* primary, bool at_front, double delta_x, bool relative)
{
	start_note_diff_command (_("resize notes"));

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		CanvasNote*  canvas_note = (*i)->canvas_note;
		SimpleRect*  resize_rect = (*i)->resize_rect;
		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x1() + delta_x;
			} else {
				current_x = primary->x1() + delta_x;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x2() + delta_x;
			} else {
				current_x = primary->x2() + delta_x;
			}
		}

		current_x = snap_pixel_to_frame (current_x);
		current_x = frames_to_beats (current_x);

		if (at_front && current_x < canvas_note->note()->end_time()) {
			note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::StartTime, current_x);

			double len = canvas_note->note()->time() - current_x;
			len += canvas_note->note()->length();

			if (len > 0) {
				/* XXX convert to beats */
				note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::Length, len);
			}
		}

		if (!at_front) {
			double len = current_x - canvas_note->note()->time();

			if (len > 0) {
				/* XXX convert to beats */
				note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::Length, len);
			}
		}

		delete resize_rect;
		delete (*i);
	}

	_resize_data.clear();
	apply_diff();
}

void
MidiRegionView::change_note_channel (CanvasNoteEvent* event, int8_t channel)
{
	note_diff_add_change (event, MidiModel::NoteDiffCommand::Channel, (uint8_t) channel);
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

        event->set_selected (event->selected()); // change color 
        
	note_diff_add_change (event, MidiModel::NoteDiffCommand::Velocity, new_velocity);
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
	note_diff_add_change (event, MidiModel::NoteDiffCommand::NoteNumber, new_note);
}

void
MidiRegionView::trim_note (CanvasNoteEvent* event, Evoral::MusicalTime front_delta, Evoral::MusicalTime end_delta)
{
	bool change_start = false;
	bool change_length = false;
	Evoral::MusicalTime new_start = 0;
	Evoral::MusicalTime new_length = 0;

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
		note_diff_add_change (event, MidiModel::NoteDiffCommand::StartTime, new_start);
	}

	if (change_length) {
		note_diff_add_change (event, MidiModel::NoteDiffCommand::Length, new_length);
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

	note_diff_add_change (event, MidiModel::NoteDiffCommand::StartTime, new_time);
}

void
MidiRegionView::change_note_length (CanvasNoteEvent* event, Evoral::MusicalTime t)
{
	note_diff_add_change (event, MidiModel::NoteDiffCommand::Length, t);
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

	start_note_diff_command (_("change velocities"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end();) {
		Selection::iterator next = i;
		++next;
		change_note_velocity (*i, delta, true);
		i = next;
	}

	apply_diff();
	
        if (!_selection.empty()) {
                char buf[24];
                snprintf (buf, sizeof (buf), "Vel %d", 
                          (int) (*_selection.begin())->note()->velocity());
                trackview.editor().show_verbose_canvas_cursor_with (buf, 10, 10);
        }
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

	start_note_diff_command (_("transpose"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_note (*i, delta, true);
		i = next;
	}

	apply_diff ();
}

void
MidiRegionView::change_note_lengths (bool fine, bool shorter, Evoral::MusicalTime delta, bool start, bool end)
{
        if (delta == 0.0) {
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
        }

	if (shorter) {
		delta = -delta;
	}

	start_note_diff_command (_("change note lengths"));

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

	framepos_t ref_point = _region->position() + beats_to_frames ((*(_selection.begin()))->note()->time());
	framepos_t unused;
	framepos_t distance;

	if (trackview.editor().snap_mode() == Editing::SnapOff) {
		
		/* grid is off - use nudge distance */

		distance = trackview.editor().get_nudge_distance (ref_point, unused);

	} else {

		/* use grid */

		framepos_t next_pos = ref_point;

		if (forward) {
			if (max_framepos - 1 < next_pos) {
                                next_pos += 1;
                        }
		} else {
			if (next_pos == 0) {
				return;
			}
			next_pos -= 1;
		}

		trackview.editor().snap_to (next_pos, (forward ? 1 : -1), false);
		distance = ref_point - next_pos;
	}

	if (distance == 0) {
		return;
	}

	Evoral::MusicalTime delta = frames_to_beats (fabs (distance));

	if (!forward) {
		delta = -delta;
	}

	start_note_diff_command (_("nudge"));

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
	start_note_diff_command(_("change channel"));
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		note_diff_add_change (*i, MidiModel::NoteDiffCommand::Channel, channel);
	}

	apply_diff();
}


void
MidiRegionView::note_entered(ArdourCanvas::CanvasNoteEvent* ev)
{
	Editor* editor = dynamic_cast<Editor*>(&trackview.editor());
        
        pre_enter_cursor = editor->get_canvas_cursor ();

	if (_mouse_state == SelectTouchDragging) {
		note_selected (ev, true);
	}

	show_verbose_canvas_cursor (ev->note ());
}

void
MidiRegionView::note_left (ArdourCanvas::CanvasNoteEvent*)
{
	Editor* editor = dynamic_cast<Editor*>(&trackview.editor());

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->hide_velocity ();
	}

	editor->hide_verbose_canvas_cursor ();

        if (pre_enter_cursor) {
                editor->set_canvas_cursor (pre_enter_cursor);
                pre_enter_cursor = 0;
        }
}

void
MidiRegionView::note_mouse_position (float x_fraction, float /*y_fraction*/, bool can_set_cursor)
{
	Editor* editor = dynamic_cast<Editor*>(&trackview.editor());

        if (x_fraction > 0.0 && x_fraction < 0.25) {
                editor->set_canvas_cursor (editor->cursors()->left_side_trim);
        } else if (x_fraction >= 0.75 && x_fraction < 1.0) {
                editor->set_canvas_cursor (editor->cursors()->right_side_trim);
        } else {
                if (pre_enter_cursor && can_set_cursor) {
                        editor->set_canvas_cursor (pre_enter_cursor);
                }
        }
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

        if (op != Copy) {

                start_note_diff_command();
                
                for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
                        switch (op) {
                        case Copy:
                                break;
                        case Cut:
                        case Clear:
                                note_diff_remove_note (*i);
                                break;
                        }
                }
                
                apply_diff();
        }
}

MidiCutBuffer*
MidiRegionView::selection_as_cut_buffer () const
{
	Notes notes;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
                NoteType* n = (*i)->note().get();
		notes.insert (boost::shared_ptr<NoteType> (new NoteType (*n)));
	}

	MidiCutBuffer* cb = new MidiCutBuffer (trackview.session());
	cb->set (notes);

	return cb;
}

void
MidiRegionView::paste (framepos_t pos, float times, const MidiCutBuffer& mcb)
{
	if (mcb.empty()) {
		return;
	}

	start_note_diff_command (_("paste"));

	Evoral::MusicalTime beat_delta;
	Evoral::MusicalTime paste_pos_beats;
	Evoral::MusicalTime duration;
	Evoral::MusicalTime end_point = 0;

	duration = (*mcb.notes().rbegin())->end_time() - (*mcb.notes().begin())->time();
	paste_pos_beats = frames_to_beats (pos - _region->position());
	beat_delta = (*mcb.notes().begin())->time() - paste_pos_beats;
	paste_pos_beats = 0;

        clear_selection ();

	for (int n = 0; n < (int) times; ++n) {

		for (Notes::const_iterator i = mcb.notes().begin(); i != mcb.notes().end(); ++i) {

			boost::shared_ptr<NoteType> copied_note (new NoteType (*((*i).get())));
			copied_note->set_time (paste_pos_beats + copied_note->time() - beat_delta);

			/* make all newly added notes selected */

			note_diff_add_note (copied_note, true);
			end_point = copied_note->end_time();
		}

		paste_pos_beats += duration;
	}

	/* if we pasted past the current end of the region, extend the region */

	framepos_t end_frame = _region->position() + beats_to_frames (end_point);
	framepos_t region_end = _region->position() + _region->length() - 1;

	if (end_frame > region_end) {

		trackview.session()->begin_reversible_command (_("paste"));

                _region->clear_changes ();
		_region->set_length (end_frame, this);
		trackview.session()->add_command (new StatefulDiffCommand (_region));
	}

	apply_diff ();
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
	// framepos_t pos = -1;
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
	// framepos_t pos = -1;
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
MidiRegionView::selection_as_notelist (Notes& selected, bool allow_all_if_none_selected)
{
        bool had_selected = false;

	time_sort_events ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((*i)->selected()) {
			selected.insert ((*i)->note());
                        had_selected = true;
		}
	}
        
        if (allow_all_if_none_selected && !had_selected) {
                for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
                        selected.insert ((*i)->note());
		}
	}
}

void
MidiRegionView::update_ghost_note (double x, double y)
{
	_last_ghost_x = x;
	_last_ghost_y = y;
	
	_note_group->w2i (x, y);
	framepos_t f = trackview.editor().pixel_to_frame (x) + _region->position ();
	trackview.editor().snap_to (f);
	f -= _region->position ();

	bool success;
	Evoral::MusicalTime beats = trackview.editor().get_grid_type_as_beats (success, f);
	if (!success) {
		beats = 1;
	}
	
	double length = frames_to_beats (snap_frame_to_frame (f + beats_to_frames (beats)) - f);
	
	_ghost_note->note()->set_time (frames_to_beats (f + _region->start()));
	_ghost_note->note()->set_length (length);
	_ghost_note->note()->set_note (midi_stream_view()->y_to_note (y));

	update_note (_ghost_note);

	show_verbose_canvas_cursor (_ghost_note->note ());
}

void
MidiRegionView::create_ghost_note (double x, double y)
{
	delete _ghost_note;
	_ghost_note = 0;

	boost::shared_ptr<NoteType> g (new NoteType);
	_ghost_note = new NoEventCanvasNote (*this, *_note_group, g);
	update_ghost_note (x, y);
	_ghost_note->show ();

	_last_ghost_x = x;
	_last_ghost_y = y;

	show_verbose_canvas_cursor (_ghost_note->note ());
}

void
MidiRegionView::snap_changed ()
{
	if (!_ghost_note) {
		return;
	}
	
	create_ghost_note (_last_ghost_x, _last_ghost_y);
}

void
MidiRegionView::show_verbose_canvas_cursor (boost::shared_ptr<NoteType> n) const
{
	char buf[24];
	snprintf (buf, sizeof (buf), "%s (%d)\nVel %d", 
                  Evoral::midi_note_name (n->note()).c_str(), 
                  (int) n->note (),
                  (int) n->velocity());
	trackview.editor().show_verbose_canvas_cursor_with (buf, 10, 20);
}

void
MidiRegionView::drop_down_keys ()
{
        _mouse_state = None;
}

void
MidiRegionView::maybe_select_by_position (GdkEventButton* ev, double /*x*/, double y)
{
	double note = midi_stream_view()->y_to_note(y);
        Events e;
	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
        
        cerr << "Selecting by position\n";

	uint16_t chn_mask = mtv->channel_selector().get_selected_channels();

        if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
                get_events (e, Evoral::Sequence<Evoral::MusicalTime>::PitchGreaterThanOrEqual, (uint8_t) floor (note), chn_mask);
        } else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
                get_events (e, Evoral::Sequence<Evoral::MusicalTime>::PitchLessThanOrEqual, (uint8_t) floor (note), chn_mask);
        } else {
                return;
        }

	bool add_mrv_selection = false;

	if (_selection.empty()) {
		add_mrv_selection = true;
	}

        for (Events::iterator i = e.begin(); i != e.end(); ++i) {
                if (_selection.insert (*i).second) {
                        (*i)->set_selected (true);
                }
	}

	if (add_mrv_selection) {
		PublicEditor& editor (trackview.editor());
		editor.get_selection().add (this);
	}
}                

void
MidiRegionView::color_handler ()
{
        RegionView::color_handler ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
                (*i)->set_selected ((*i)->selected()); // will change color
        }

        /* XXX probably more to do here */
}

void
MidiRegionView::enable_display (bool yn)
{
	RegionView::enable_display (yn);
	if (yn) {
		redisplay_model ();
	}
}

void
MidiRegionView::show_step_edit_cursor (Evoral::MusicalTime pos)
{
        if (_step_edit_cursor == 0) {
                ArdourCanvas::Group* const group = (ArdourCanvas::Group*)get_canvas_group();

                _step_edit_cursor = new ArdourCanvas::SimpleRect (*group);
                _step_edit_cursor->property_y1() = 0;
                _step_edit_cursor->property_y2() = midi_stream_view()->contents_height();
                _step_edit_cursor->property_fill_color_rgba() = RGBA_TO_UINT (45,0,0,90);
                _step_edit_cursor->property_outline_color_rgba() = RGBA_TO_UINT (85,0,0,90);
        }

        move_step_edit_cursor (pos);
        _step_edit_cursor->show ();
}

void
MidiRegionView::move_step_edit_cursor (Evoral::MusicalTime pos)
{
        _step_edit_cursor_position = pos;

        if (_step_edit_cursor) {
                double pixel = trackview.editor().frame_to_pixel (beats_to_frames (pos));
                _step_edit_cursor->property_x1() = pixel;
                set_step_edit_cursor_width (_step_edit_cursor_width);
        }
}

void
MidiRegionView::hide_step_edit_cursor ()
{
        if (_step_edit_cursor) {
                _step_edit_cursor->hide ();
        }
}

void
MidiRegionView::set_step_edit_cursor_width (Evoral::MusicalTime beats)
{
        _step_edit_cursor_width = beats;

        if (_step_edit_cursor) {
                _step_edit_cursor->property_x2() = _step_edit_cursor->property_x1() + trackview.editor().frame_to_pixel (beats_to_frames (beats));
        }
}

/** Called when a diskstream on our track has received some data.  Update the view, if applicable.
 *  @param buf Data that has been recorded.
 *  @param w Source that this data will end up in.
 */
void
MidiRegionView::data_recorded (boost::shared_ptr<MidiBuffer> buf, boost::weak_ptr<MidiSource> w)
{
	if (!_active_notes) {
		/* we aren't actively being recorded to */
		return;
	}
	
	boost::shared_ptr<MidiSource> src = w.lock ();
	if (!src || src != midi_region()->midi_source()) {
		/* recorded data was not destined for our source */
		return;
	}

	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*> (&trackview);
	BeatsFramesConverter converter (trackview.session()->tempo_map(), mtv->midi_track()->get_capture_start_frame (0));

	framepos_t back = max_framepos;
	
	for (MidiBuffer::iterator i = buf->begin(); i != buf->end(); ++i) {
		Evoral::MIDIEvent<MidiBuffer::TimeType> const ev (*i, false);
		assert (ev.buffer ());

		Evoral::MusicalTime const time_beats = converter.from (ev.time () - converter.origin_b ());

		if (ev.type() == MIDI_CMD_NOTE_ON) {

			boost::shared_ptr<Evoral::Note<Evoral::MusicalTime> > note (
				new Evoral::Note<Evoral::MusicalTime> (ev.channel(), time_beats, 0, ev.note(), ev.velocity())
				);

			add_note (note, true);

			/* fix up our note range */
			if (ev.note() < _current_range_min) {
				midi_stream_view()->apply_note_range (ev.note(), _current_range_max, true);
			} else if (ev.note() > _current_range_max) {
				midi_stream_view()->apply_note_range (_current_range_min, ev.note(), true);
			}
			
		} else if (ev.type() == MIDI_CMD_NOTE_OFF) {
			resolve_note (ev.note (), time_beats);
		}

		back = ev.time ();
	}

	midi_stream_view()->check_record_layers (region(), back);
}

void
MidiRegionView::trim_front_starting ()
{
	/* Reparent the note group to the region view's parent, so that it doesn't change
	   when the region view is trimmed.
	*/
	_temporary_note_group = new ArdourCanvas::Group (*group->property_parent ());
	_temporary_note_group->move (group->property_x(), group->property_y());
	_note_group->reparent (*_temporary_note_group);
}

void
MidiRegionView::trim_front_ending ()
{
	_note_group->reparent (*group);
	delete _temporary_note_group;
	_temporary_note_group = 0;

	if (_region->start() < 0) {
		/* Trim drag made start time -ve; fix this */
		midi_region()->fix_negative_start ();
	}
}
