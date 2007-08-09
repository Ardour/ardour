/*
    Copyright (C) 2001-2006 Paul Davis 

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

#ifndef __gtk_ardour_midi_region_view_h__
#define __gtk_ardour_midi_region_view_h__

#include <vector>

#include <libgnomecanvasmm.h>
#include <libgnomecanvasmm/polygon.h>
#include <ardour/midi_track.h>
#include <ardour/midi_region.h>
#include <ardour/midi_model.h>
#include <ardour/diskstream.h>
#include <ardour/types.h>

#include "region_view.h"
#include "midi_time_axis.h"
#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"
#include "canvas.h"
#include "canvas-note.h"
#include "canvas-midi-event.h"

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
};

class MidiTimeAxisView;
class GhostRegion;
class AutomationTimeAxisView;

class MidiRegionView : public RegionView
{
  public:
	MidiRegionView (ArdourCanvas::Group *, 
	                RouteTimeAxisView&,
	                boost::shared_ptr<ARDOUR::MidiRegion>,
	                double initial_samples_per_unit,
	                Gdk::Color& basic_color);

	~MidiRegionView ();
	
	virtual void init (Gdk::Color& basic_color, bool wfd);
	
	inline const boost::shared_ptr<ARDOUR::MidiRegion> midi_region() const
		{ return boost::dynamic_pointer_cast<ARDOUR::MidiRegion>(_region); }

	inline MidiTimeAxisView* midi_view() const
		{ return dynamic_cast<MidiTimeAxisView*>(&trackview); }

	inline MidiStreamView* midi_stream_view() const
		{ return midi_view()->midi_view(); }
	
	void set_y_position_and_height (double, double);
	
	void redisplay_model();

    GhostRegion* add_ghost (AutomationTimeAxisView&);

	void add_event(const ARDOUR::MidiEvent& ev);
	void add_note(const ARDOUR::MidiModel::Note& note);

	void begin_write();
	void end_write();
	void extend_active_notes();

	void create_note_at(double x, double y, double dur);

	void display_model(boost::shared_ptr<ARDOUR::MidiModel> model);

	/* This stuff is a bit boilerplatey ATM.  Work in progress. */

	inline void start_remove_command() {
		if (!_delta_command)
			_delta_command = _model->new_delta_command();
	}
	
	inline void start_delta_command() {
		if (!_delta_command)
			_delta_command = _model->new_delta_command();
	}

	void command_remove_note(ArdourCanvas::CanvasMidiEvent* ev) {
		if (_delta_command && ev->note()) {
			_delta_command->remove(*ev->note());
			ev->selected(true);
		}
	}
	
	void command_add_note(ARDOUR::MidiModel::Note& note) {
		if (_delta_command) {
			_delta_command->add(note);
		}
	}

	void note_entered(ArdourCanvas::CanvasMidiEvent* ev) {
		cerr << "ENTERED, STATE = " << _mouse_state << endl;
		if (_mouse_state == EraseDragging) {
			start_delta_command();
			ev->selected(true);
			_delta_command->remove(*ev->note());
		}
	}

	void abort_command() {
		delete _delta_command;
		_delta_command = NULL;
		clear_selection();
	}

	void apply_command() {
		if (_delta_command) {
			_model->apply_command(_delta_command);
			_delta_command = NULL;
		}
		midi_view()->midi_track()->diskstream()->playlist_modified();
	}

	void   unique_select(ArdourCanvas::CanvasMidiEvent* ev);
	void   note_selected(ArdourCanvas::CanvasMidiEvent* ev, bool add);
	void   note_deselected(ArdourCanvas::CanvasMidiEvent* ev, bool add);
	void   delete_selection();
	size_t selection_size() { return _selection.size(); }

	void move_selection(double dx, double dy);
	void note_dropped(ArdourCanvas::CanvasMidiEvent* ev, double dt, uint8_t dnote);

  protected:

    /* this constructor allows derived types
       to specify their visibility requirements
       to the TimeAxisViewItem parent class
    */
    
    MidiRegionView (ArdourCanvas::Group *, 
	                RouteTimeAxisView&,
	                boost::shared_ptr<ARDOUR::MidiRegion>,
	                double samples_per_unit,
	                Gdk::Color& basic_color,
	                TimeAxisViewItem::Visibility);
    
    void region_resized (ARDOUR::Change);

    void set_flags (XMLNode *);
    void store_flags ();
    
	void reset_width_dependent_items (double pixel_width);

  private:

	void clear_events();

	bool canvas_event(GdkEvent* ev);
	bool note_canvas_event(GdkEvent* ev);
	
	void clear_selection_except(ArdourCanvas::CanvasMidiEvent* ev);
	void clear_selection() { clear_selection_except(NULL); }
	void update_drag_selection(double last_x, double x, double last_y, double y);

	double _default_note_length;

	boost::shared_ptr<ARDOUR::MidiModel>        _model;
	std::vector<ArdourCanvas::CanvasMidiEvent*> _events;
	ArdourCanvas::CanvasNote**                  _active_notes;
	ArdourCanvas::Group*                        _note_group;
	ARDOUR::MidiModel::DeltaCommand*            _delta_command;
	
	enum MouseState { None, Pressed, SelectDragging, AddDragging, EraseDragging };
	MouseState _mouse_state;
	int _pressed_button;

	typedef std::set<ArdourCanvas::CanvasMidiEvent*> Selection;
	Selection _selection;
};

#endif /* __gtk_ardour_midi_region_view_h__ */
