/*
 * Copyright (C) 2005-2009 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_time_axis_view_item_h__
#define __gtk_ardour_time_axis_view_item_h__

#include <string>
#include <gdk/gdk.h>
#include <gdkmm/color.h>
#include <pangomm/fontdescription.h>
#include "pbd/signals.h"
#include "selectable.h"

class TimeAxisView;

namespace ArdourCanvas {
	class Pixbuf;
	class Rectangle;
	class Item;
	class Container;
	class Text;
}

using ARDOUR::samplepos_t;
using ARDOUR::samplecnt_t;

/**
 * Base class for items that may appear upon a TimeAxisView.
 */

class TimeAxisViewItem : public Selectable, public PBD::ScopedConnectionList
{
public:
	virtual ~TimeAxisViewItem();

	virtual bool set_position(Temporal::timepos_t const &, void*, double* delta = 0);
	Temporal::timepos_t get_position() const;
	virtual bool set_duration(Temporal::timecnt_t const &, void*);
	Temporal::timecnt_t get_duration() const;
	virtual void set_max_duration(Temporal::timecnt_t const &, void*);
	Temporal::timecnt_t get_max_duration() const;
	virtual void set_min_duration(Temporal::timecnt_t const &, void*);
	Temporal::timecnt_t get_min_duration() const;
	virtual void set_position_locked(bool, void*);
	bool get_position_locked() const;
	void set_max_duration_active(bool, void*);
	bool get_max_duration_active() const;
	void set_min_duration_active(bool, void*);
	bool get_min_duration_active() const;
	void set_item_name(std::string, void*);
	virtual std::string get_item_name() const;
	virtual void set_selected(bool yn);
	void set_sensitive (bool yn) { _sensitive = yn; }
	bool sensitive () const { return _sensitive; }
	TimeAxisView& get_time_axis_view () const;
	void set_name_text(const std::string&);
	virtual void set_height(double h);
	virtual double height() const { return _height; }
	void set_y (double);
	void set_color (uint32_t);
	void set_name_text_color ();

	virtual void     set_frame_color();
	virtual uint32_t get_fill_color () const;

	ArdourCanvas::Item* get_canvas_frame();
	ArdourCanvas::Item* get_canvas_group() const;
	ArdourCanvas::Item* get_name_highlight();

	virtual void set_samples_per_pixel (double);

	double get_samples_per_pixel () const;

	virtual void drag_start();
	virtual void drag_end();
	bool dragging() const { return _dragging; }

	virtual void visual_layer_on_top() {}
	virtual void raise () {}
	virtual void raise_to_top () {}
	virtual void lower () {}
	virtual void lower_to_bottom () {}

	/** @return true if the name area should respond to events */
	bool name_active() const { return name_connected; }

	// Default sizes, font and spacing
	static Pango::FontDescription NAME_FONT;
	static void set_constant_heights ();
	static const double NAME_X_OFFSET;
	static const double GRAB_HANDLE_TOP;
	static const double GRAB_HANDLE_WIDTH;

	/* these are not constant, but vary with the pixel size
	 * of the font used to display the item name.
	 */
	static int    NAME_HEIGHT;
	static double NAME_Y_OFFSET;
	static double NAME_HIGHLIGHT_SIZE;
	static double NAME_HIGHLIGHT_THRESH;

	/**
	 * Emitted when this Group has been removed.
	 * This is different to the CatchDeletion signal in that this signal
	 * is emitted during the deletion of this Time Axis, and not during
	 * the destructor, this allows us to capture the source of the deletion
	 * event
	 */

	sigc::signal<void,std::string,void*> ItemRemoved;

	enum Visibility {
		ShowFrame = 0x1,
		ShowNameHighlight = 0x2,
		ShowNameText = 0x4,
		ShowHandles = 0x8,
		HideFrameLeft = 0x10,
		HideFrameRight = 0x20,
		HideFrameTB = 0x40,
		FullWidthNameHighlight = 0x80
	};

	virtual void update_visibility () {}

protected:
	TimeAxisViewItem (const std::string &, ArdourCanvas::Item&, TimeAxisView&, double, uint32_t fill_color,
	                  Temporal::timepos_t const &, Temporal::timecnt_t const &, bool recording = false, bool automation = false, Visibility v = Visibility (0));

	TimeAxisViewItem (const TimeAxisViewItem&);

	void init (ArdourCanvas::Item*, double, uint32_t, Temporal::timepos_t const &, Temporal::timecnt_t const &, Visibility, bool, bool);

	virtual bool canvas_group_event (GdkEvent*);

	virtual void set_colors();
	virtual void set_frame_gradient ();

	void set_trim_handle_colors();

	virtual void reset_width_dependent_items (double);

	static gint idle_remove_this_item(TimeAxisViewItem*, void*);

	/** time axis that this item is on */
	TimeAxisView& trackview;

	/** indicates whether this item is locked to its current position */
	bool position_locked;

	/** position of this item on the timeline */
	Temporal::timepos_t time_position;

	/** duration of this item upon the timeline */
	Temporal::timecnt_t item_duration;

	/** maximum duration that this item can have */
	Temporal::timecnt_t max_item_duration;

	/** minimum duration that this item can have */
	Temporal::timecnt_t min_item_duration;

	/** indicates whether the max duration constraint is active */
	bool max_duration_active;

	/** indicates whether the min duration constraint is active */
	bool min_duration_active;

	/** samples per canvas pixel */
	double samples_per_pixel;

	/** should the item respond to events */
	bool _sensitive;

	/**
	 * The unique item name of this Item.
	 * Each item upon a time axis must have a unique id.
	 */
	std::string item_name;

	/** true if the name should respond to events */
	bool name_connected;

	uint32_t fill_color;

	uint32_t last_item_width;
	int name_text_width;
	bool wide_enough_for_name;
	bool high_enough_for_name;

	ArdourCanvas::Container* group;

	ArdourCanvas::Rectangle* frame;
	ArdourCanvas::Rectangle* selection_frame;
	ArdourCanvas::Text*      name_text;
	ArdourCanvas::Rectangle* name_highlight;

	/* with these two values, if frame_handle_start == 0 then frame_handle_end will also be 0 */
	ArdourCanvas::Rectangle* frame_handle_start; ///< `frame' (fade) handle for the start of the item, or 0
	ArdourCanvas::Rectangle* frame_handle_end; ///< `frame' (fade) handle for the end of the item, or 0

	bool frame_handle_crossing (GdkEvent*, ArdourCanvas::Rectangle*);

	double _height;
	Visibility visibility;
	std::string fill_color_name;
	bool _recregion;
	bool _automation; ///< true if this is an automation region view
	bool _dragging;
	double _width;

	void manage_name_text ();

private:
	void parameter_changed (std::string);
	void manage_name_highlight ();

}; /* class TimeAxisViewItem */

#endif /* __gtk_ardour_time_axis_view_item_h__ */
