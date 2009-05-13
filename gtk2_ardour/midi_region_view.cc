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

#include "ardour/playlist.h"
#include "ardour/tempo.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_model.h"
#include "ardour/midi_patch_manager.h"

#include "evoral/Parameter.hpp"
#include "evoral/Control.hpp"

#include "streamview.h"
#include "midi_region_view.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "simpleline.h"
#include "canvas-hit.h"
#include "canvas-note.h"
#include "canvas-program-change.h"
#include "public_editor.h"
#include "ghostregion.h"
#include "midi_time_axis.h"
#include "automation_time_axis.h"
#include "automation_region_view.h"
#include "utils.h"
#include "midi_util.h"
#include "gui_thread.h"
#include "keyboard.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

MidiRegionView::MidiRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv,
		boost::shared_ptr<MidiRegion> r, double spu, Gdk::Color& basic_color)
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
	, _delta_command(NULL)
	, _mouse_state(None)
	, _pressed_button(0)
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
	, _delta_command(NULL)
	, _mouse_state(None)
	, _pressed_button(0)
	
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
	, _delta_command(NULL)
	, _mouse_state(None)
	, _pressed_button(0)
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
	, _delta_command(NULL)
	, _mouse_state(None)
	, _pressed_button(0)
{
	Gdk::Color c;
	int r,g,b,a;

	UINT_TO_RGBA (other.fill_color, &r, &g, &b, &a);
	c.set_rgb_p (r/255.0, g/255.0, b/255.0);

	init (c, true);
}

void
MidiRegionView::init (Gdk::Color& basic_color, bool wfd)
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
			redisplay_model();
		}
		_model->ContentsChanged.connect(sigc::mem_fun(this, &MidiRegionView::redisplay_model));
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
	static double drag_start_x, drag_start_y;
	static double last_x, last_y;
	double event_x, event_y;
	nframes64_t event_frame = 0;

	static ArdourCanvas::SimpleRect* drag_rect = NULL;

	if (trackview.editor().current_mouse_mode() != MouseNote)
		return false;
	
	const Editing::MidiEditMode midi_edit_mode = trackview.editor().current_midi_edit_mode();

	switch (ev->type) {
	case GDK_KEY_PRESS:
		if (ev->key.keyval == GDK_Shift_L || ev->key.keyval == GDK_Control_L) {
			_mouse_state = SelectTouchDragging;
			return true;
		} else if (ev->key.keyval == GDK_Escape) {
			clear_selection();
			_mouse_state = None;
		}
		return false;

	case GDK_KEY_RELEASE:
		if (ev->key.keyval == GDK_Delete) {
			delete_selection();
			apply_command();
			return true;
		} else if (ev->key.keyval == GDK_Shift_L || ev->key.keyval == GDK_Control_L) {
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
			if (_pressed_button == 1 && midi_edit_mode == MidiEditSelect) {
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
			} else if (midi_edit_mode == MidiEditPencil) {
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

		if (_pressed_button != 1) {
			return false;
		}
			
		switch (_mouse_state) {
		case Pressed: // Clicked
			switch (midi_edit_mode) {
			case MidiEditSelect:
			case MidiEditResize:
				clear_selection();
				break;
			case MidiEditPencil:
				create_note_at(event_x, event_y, _default_note_length);
			default: break;
			}
			_mouse_state = None;
			break;
		case SelectRectDragging: // Select drag done
			_mouse_state = None;
			delete drag_rect;
			drag_rect = NULL;
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
			drag_rect = NULL;
		default: break;
		}

	default: break;
	}

	return false;
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
}


void
MidiRegionView::display_model(boost::shared_ptr<MidiModel> model)
{
	_model = model;
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
MidiRegionView::command_add_note(const boost::shared_ptr<NoteType> note, bool selected, bool show_velocity)
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
MidiRegionView::command_remove_note(ArdourCanvas::CanvasNoteEvent* ev)
{
	if (_delta_command && ev->note()) {
		_delta_command->remove(ev->note());
	}
}
	
void
MidiRegionView::apply_command()
{
	if (!_delta_command) {
		return;
	}

	// Mark all selected notes for selection when model reloads
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		_marked_for_selection.insert((*i)->note());
	}
	
	_model->apply_command(trackview.session(), _delta_command);
	_delta_command = NULL; 
	midi_view()->midi_track()->diskstream()->playlist_modified();

	_marked_for_selection.clear();
	_marked_for_velocity.clear();
}
	

void
MidiRegionView::abort_command()
{
	delete _delta_command;
	_delta_command = NULL;
	clear_selection();
}


void
MidiRegionView::redisplay_model()
{
	// Don't redisplay the model if we're currently recording and displaying that
	if (_active_notes) {
		return;
	}

	if (_model) {
		clear_events();
		_model->read_lock();
		
		MidiModel::Notes notes = _model->notes();
		
		for (size_t i = 0; i < _model->n_notes(); ++i) {
			add_note(_model->note_at(i));
		}
		
		display_sysexes();

		display_program_changes();

		_model->read_unlock();

	} else {
		cerr << "MidiRegionView::redisplay_model called without a model" << endmsg;
	}
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
		ARDOUR::MidiModel::TimeType time = (*i)->time();
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
		Item* item = dynamic_cast<Item*>(event);
		assert(item);
		if (event && event->note()) {
			if (event->note()->note() < _current_range_min
					|| event->note()->note() > _current_range_max) {
				if (canvas_item_visible(item)) {
					item->hide();
				}
			} else {
				if (!canvas_item_visible(item)) {
					item->show();
				}

				if (CanvasNote* note = dynamic_cast<CanvasNote*>(event)) {
					const double y1 = midi_stream_view()->note_to_y(event->note()->note());
					const double y2 = y1 + floor(midi_stream_view()->note_height());

					note->property_y1() = y1;
					note->property_y2() = y2;
				} else if (CanvasHit* hit = dynamic_cast<CanvasHit*>(event)) {
					double x = trackview.editor().frame_to_pixel(
							beats_to_frames(event->note()->time()) - _region->start());
					const double diamond_size = midi_stream_view()->note_height() / 2.0;
					double y = midi_stream_view()->note_to_y(event->note()->note()) 
					                 + ((diamond_size-2.0) / 4.0);
					
					hit->set_height(diamond_size);
					hit->move(x-hit->x1(), y-hit->y1());
					hit->show();
				}
			}
		}
	}
	
}

GhostRegion*
MidiRegionView::add_ghost (TimeAxisView& tv)
{
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&trackview);
	CanvasNote* note;
	assert(rtv);

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
		_active_notes[i] = NULL;
	}
}


/** Destroy note state for add_event
 */
void
MidiRegionView::end_write()
{
	delete[] _active_notes;
	_active_notes = NULL;
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
		_active_notes[note] = NULL;
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
MidiRegionView::note_in_visible_range(const boost::shared_ptr<NoteType> note) const
{
	const nframes64_t note_start_frames = beats_to_frames(note->time());
	bool outside = (note_start_frames - _region->start() >= _region->length())
			|| (note_start_frames < _region->start())
			|| (note->note() < midi_stream_view()->lowest_note())
			|| (note->note() > midi_stream_view()->highest_note());
	return !outside;
}

/** Add a MIDI note to the view (with length).
 *
 * If in sustained mode, notes with length 0 will be considered active
 * notes, and resolve_note should be called when the corresponding note off
 * event arrives, to properly display the note.
 */
void
MidiRegionView::add_note(const boost::shared_ptr<NoteType> note)
{
	assert(note->time() >= 0);
	assert(midi_view()->note_mode() == Sustained || midi_view()->note_mode() == Percussive);
	
	const nframes64_t note_start_frames = beats_to_frames(note->time());
	const nframes64_t note_end_frames   = beats_to_frames(note->end_time());

	ArdourCanvas::Group* const group = (ArdourCanvas::Group*)get_canvas_group();

	CanvasNoteEvent* event = 0;
	
	const double x = trackview.editor().frame_to_pixel(note_start_frames - _region->start());
	
	if (midi_view()->note_mode() == Sustained) {
		const double y1 = midi_stream_view()->note_to_y(note->note());
		const double note_endpixel = 
			trackview.editor().frame_to_pixel(note_end_frames - _region->start());
		
		CanvasNote* ev_rect = new CanvasNote(*this, *group, note);
		ev_rect->property_x1() = x;
		ev_rect->property_y1() = y1;
		if (note->length() > 0) {
			ev_rect->property_x2() = note_endpixel;
		} else {
			ev_rect->property_x2() = trackview.editor().frame_to_pixel(_region->length());
		}
		ev_rect->property_y2() = y1 + floor(midi_stream_view()->note_height());

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
				_active_notes[note->note()] = ev_rect;
			}
			/* outline all but right edge */
			ev_rect->property_outline_what() = (guint32) (0x1 & 0x4 & 0x8);
		} else {
			/* outline all edges */
			ev_rect->property_outline_what() = (guint32) 0xF;
		}

		event = ev_rect;

		MidiGhostRegion* gr;
		for (std::vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
			if ((gr = dynamic_cast<MidiGhostRegion*>(*g)) != 0) {
				gr->add_note(ev_rect);
			}
		}

	} else if (midi_view()->note_mode() == Percussive) {
		const double diamond_size = midi_stream_view()->note_height() / 2.0;
		const double y = midi_stream_view()->note_to_y(note->note()) + ((diamond_size-2) / 4.0);

		CanvasHit* ev_diamond = new CanvasHit(*this, *group, diamond_size, note);
		ev_diamond->move(x, y);
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
		if (note_in_visible_range(note)) {
			event->show();
		} else {
			event->hide();
		}
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

	if (!_delta_command) {
		_delta_command = _model->new_delta_command("delete selection");
	}

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->selected()) {
			_delta_command->remove((*i)->note());
		}
	}

	_selection.clear();
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
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i) != ev) {
			(*i)->selected(false);
			(*i)->hide_velocity();
		}
	}

	_selection.clear();
	_selection.insert(ev);

	if ( ! ev->selected()) {
		ev->selected(true);
	}
}

void
MidiRegionView::note_selected(ArdourCanvas::CanvasNoteEvent* ev, bool add)
{
	if ( ! add) {
		clear_selection_except(ev);
	}

	if (_selection.insert(ev).second) {
		play_midi_note(ev->note());
	}

	if ( ! ev->selected()) {
		ev->selected(true);
	}
}


void
MidiRegionView::note_deselected(ArdourCanvas::CanvasNoteEvent* ev, bool add)
{
	if ( ! add) {
		clear_selection_except(ev);
	}

	_selection.erase(ev);

	if (ev->selected()) {
		ev->selected(false);
	}
}


void
MidiRegionView::update_drag_selection(double x1, double x2, double y1, double y2)
{
	const double last_y = std::min(y1, y2);
	const double y      = std::max(y1, y2);

	// TODO: Make this faster by storing the last updated selection rect, and only
	// adjusting things that are in the area that appears/disappeared.
	// We probably need a tree to be able to find events in O(log(n)) time.

#ifndef NDEBUG
	double last_x1 = 0.0;
#endif

	if (x1 < x2) {
		for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
#ifndef NDEBUG
			// Events should always be sorted by increasing x1() here
			assert((*i)->x1() >= last_x1);
			last_x1 = (*i)->x1();
#endif
			// Inside rectangle
			if ((*i)->x1() >= x1 && (*i)->x1() <= x2 && (*i)->y1() >= last_y && (*i)->y1() <= y) {
				if (!(*i)->selected()) {
					(*i)->selected(true);
					_selection.insert(*i);
					play_midi_note((*i)->note());
				}
			// Not inside rectangle
			} else if ((*i)->selected()) {
				(*i)->selected(false);
				_selection.erase(*i);
			}
		}
	} else {
		for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
#ifndef NDEBUG
			// Events should always be sorted by increasing x1() here
			assert((*i)->x1() >= last_x1);
			last_x1 = (*i)->x1();
#endif
			// Inside rectangle
			if ((*i)->x2() <= x1 && (*i)->x2() >= x2 && (*i)->y1() >= last_y && (*i)->y1() <= y) {
				if (!(*i)->selected()) {
					(*i)->selected(true);
					_selection.insert(*i);
					play_midi_note((*i)->note());
				}
			// Not inside rectangle
			} else if ((*i)->selected()) {
				(*i)->selected(false);
				_selection.erase(*i);
			}
		}
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
MidiRegionView::note_dropped(CanvasNoteEvent* ev, double dt, uint8_t dnote)
{
	// TODO: This would be faster/nicer with a MoveCommand that doesn't need to copy...
	if (_selection.find(ev) == _selection.end()) {
		return;
	}

	uint8_t lowest_note_in_selection  = midi_stream_view()->lowest_note();
	uint8_t highest_note_in_selection = midi_stream_view()->highest_note();
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
	if (dnote <= 127 && (highest_note_in_selection + dnote > 127)) {
		highest_note_difference = highest_note_in_selection - 127;
	}
	
	start_delta_command(_("move notes"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end() ; ) {
		Selection::iterator next = i;
		++next;

		const boost::shared_ptr<NoteType> copy(new NoteType(*(*i)->note().get()));

		nframes64_t start_frames = beats_to_frames((*i)->note()->time());
		if (dt >= 0) {
			start_frames += snap_frame_to_frame(trackview.editor().pixel_to_frame(dt));
		} else {
			start_frames -= snap_frame_to_frame(trackview.editor().pixel_to_frame(-dt));
		}

		copy->set_time(frames_to_beats(start_frames));

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

		copy->set_note(new_pitch);
		
		command_remove_note(*i);
		command_add_note(copy, (*i)->selected());

		i = next;
	}

	apply_command();
	
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
MidiRegionView::begin_resizing(CanvasNote::NoteEnd note_end)
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

			if (note_end == CanvasNote::NOTE_ON) {
				resize_data->current_x = note->x1();
			} else { // NOTE_OFF
				resize_data->current_x = note->x2();
			}

			_resize_data.push_back(resize_data);
		}
	}
}

void
MidiRegionView::update_resizing(CanvasNote::NoteEnd note_end, double x, bool relative)
{
	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		SimpleRect* resize_rect = (*i)->resize_rect;
		CanvasNote* canvas_note = (*i)->canvas_note;

		const double region_start = get_position_pixels();

		if (relative) {
			(*i)->current_x = (*i)->current_x + x;
		} else {
			// x is in track relative, transform it to region relative
			(*i)->current_x = x - region_start;
		}

		double current_x = (*i)->current_x;

		if (note_end == CanvasNote::NOTE_ON) {
			resize_rect->property_x1() = snap_to_pixel(current_x);
			resize_rect->property_x2() = canvas_note->x2();
		} else {
			resize_rect->property_x2() = snap_to_pixel(current_x);
			resize_rect->property_x1() = canvas_note->x1();
		}
	}
}

void
MidiRegionView::commit_resizing(CanvasNote::NoteEnd note_end, double event_x, bool relative)
{
	start_delta_command(_("resize notes"));

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		CanvasNote*  canvas_note = (*i)->canvas_note;
		SimpleRect*  resize_rect = (*i)->resize_rect;
		double       current_x   = (*i)->current_x;
		const double position    = get_position_pixels();

		if (!relative) {
			// event_x is in track relative, transform it to region relative
			current_x = event_x - position;
		}

		// because snapping works on world coordinates we have to transform current_x
		// to world coordinates before snapping and transform it back afterwards
		nframes64_t current_frame = snap_pixel_to_frame(current_x);
		// transform to region start relative
		current_frame += _region->start();
		
		const boost::shared_ptr<NoteType> copy(new NoteType(*(canvas_note->note().get())));

		// resize beginning of note
		if (note_end == CanvasNote::NOTE_ON && current_frame < copy->end_time()) {
			command_remove_note(canvas_note);
			copy->on_event().time() = current_frame;
			command_add_note(copy, _selection.find(canvas_note) != _selection.end());
		}
		// resize end of note
		if (note_end == CanvasNote::NOTE_OFF && current_frame > copy->time()) {
			command_remove_note(canvas_note);
			copy->off_event().time() = current_frame;
			command_add_note(copy, _selection.find(canvas_note) != _selection.end());
		}

		delete resize_rect;
		delete (*i);
	}

	_resize_data.clear();
	apply_command();
}

void
MidiRegionView::change_note_velocity(CanvasNoteEvent* event, int8_t velocity, bool relative)
{
	const boost::shared_ptr<NoteType> copy(new NoteType(*(event->note().get())));

	if (relative) {
		uint8_t new_velocity = copy->velocity() + velocity;
		clamp_to_0_127(new_velocity);
		copy->set_velocity(new_velocity);
	} else {
		copy->set_velocity(velocity);			
	}

	command_remove_note(event);
	command_add_note(copy, event->selected(), true);
}

void
MidiRegionView::change_velocity(CanvasNoteEvent* ev, int8_t velocity, bool relative)
{
	start_delta_command(_("change velocity"));
	
	change_note_velocity(ev, velocity, relative);

	for (Selection::iterator i = _selection.begin(); i != _selection.end();) {
		Selection::iterator next = i;
		++next;
		if ( !(*((*i)->note()) == *(ev->note())) ) {
			change_note_velocity(*i, velocity, relative);
		}
		i = next;
	}
	
	apply_command();
}

void
MidiRegionView::change_channel(uint8_t channel)
{
	start_delta_command(_("change channel"));
	for (Selection::iterator i = _selection.begin(); i != _selection.end();) {
		Selection::iterator next = i;
		++next;

		CanvasNoteEvent* event = *i;
		const boost::shared_ptr<NoteType> copy(new NoteType(*(event->note().get())));

		copy->set_channel(channel);
		
		command_remove_note(event);
		command_add_note(copy, event->selected());
		
		i = next;
	}
	
	apply_command();
}


void
MidiRegionView::note_entered(ArdourCanvas::CanvasNoteEvent* ev)
{
	if (_mouse_state == SelectTouchDragging) {
		note_selected(ev, true);
	}
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

