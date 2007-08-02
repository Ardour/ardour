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
#include <ardour/midi_region.h>
#include <ardour/midi_model.h>
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
	
	inline uint8_t contents_note_range() const
		{ return midi_stream_view()->highest_note() - midi_stream_view()->lowest_note() + 1; }

	inline double footer_height() const
		{ return name_highlight->property_y2() - name_highlight->property_y1(); }

	inline double contents_height() const
		{ return (trackview.height - footer_height() - 5.0); }
	
	inline double note_height() const
		{ return contents_height() / (double)contents_note_range(); }
			
	inline double note_to_y(uint8_t note) const
		{ return trackview.height
				- (contents_height() * (note - midi_stream_view()->lowest_note() + 1))
				- footer_height() - 3.0; }
	
	inline uint8_t y_to_note(double y) const
		{ return (uint8_t)floor((contents_height() - y)
				/ contents_height() * (double)contents_note_range()); }
	
	void set_y_position_and_height (double, double);
    
    void show_region_editor ();

    GhostRegion* add_ghost (AutomationTimeAxisView&);

	void add_event(const ARDOUR::MidiEvent& ev);
	void add_note(const ARDOUR::MidiModel::Note& note);

	void begin_write();
	void end_write();
	void extend_active_notes();

	void create_note_at(double x, double y);

	void display_model(boost::shared_ptr<ARDOUR::MidiModel> model);

	/* This stuff is a bit boilerplatey ATM.  Work in progress. */

	inline void start_remove_command() {
		_command_mode = Remove;
		if (!_delta_command)
			_delta_command = _model->new_delta_command();
	}
	
	inline void start_delta_command() {
		_command_mode = Delta;
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
		if (_command_mode == Remove && _delta_command && ev->note())
			_delta_command->remove(*ev->note());
	}

	//ARDOUR::MidiModel::DeltaCommand* delta_command() { return _delta_command; }

	void abort_command() {
		delete _delta_command;
		_delta_command = NULL;
		_command_mode = None;
	}

	void apply_command() {
		if (_delta_command) {
			_model->apply_command(_delta_command);
			_delta_command = NULL;
		}
		_command_mode = None;
	}

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

	void redisplay_model();
	void clear_events();

	bool canvas_event(GdkEvent* ev);
	bool note_canvas_event(GdkEvent* ev);

	boost::shared_ptr<ARDOUR::MidiModel> _model;
	std::vector<ArdourCanvas::Item*>     _events;
	ArdourCanvas::CanvasNote**           _active_notes;
	ARDOUR::MidiModel::DeltaCommand*     _delta_command;
	
	enum CommandMode { None, Remove, Delta };
	CommandMode _command_mode;

};

#endif /* __gtk_ardour_midi_region_view_h__ */
