/*
    Copyright (C) 2003 Paul Davis

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
	class Group;
	class Text;
}

using ARDOUR::framepos_t;
using ARDOUR::framecnt_t;

/**
 * Base class for items that may appear upon a TimeAxisView.
 */

class TimeAxisViewItem : public Selectable, public PBD::ScopedConnectionList
{
   public:
	virtual ~TimeAxisViewItem();

	virtual bool set_position(framepos_t, void*, double* delta = 0);
	framepos_t get_position() const;
	virtual bool set_duration(framecnt_t, void*);
	framecnt_t get_duration() const;
	virtual void set_max_duration(framecnt_t, void*);
	framecnt_t get_max_duration() const;
	virtual void set_min_duration(framecnt_t, void*);
	framecnt_t get_min_duration() const;
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
	void set_y (double);
	void set_color (Gdk::Color const &);

        uint32_t get_fill_color () const;

	ArdourCanvas::Item* get_canvas_frame();
	ArdourCanvas::Group* get_canvas_group();
	ArdourCanvas::Item* get_name_highlight();

	virtual void set_samples_per_pixel (double);

	double get_samples_per_pixel () const;

	virtual void drag_start() { _dragging = true; }
	virtual void drag_end() { _dragging = false; }
	bool dragging() const { return _dragging; }

	virtual void raise () { return; }
	virtual void raise_to_top () { return; }
	virtual void lower () { return; }
	virtual void lower_to_bottom () { return; }

        virtual void hide_rect ();
        virtual void show_rect ();

	/** @return true if the name area should respond to events */
	bool name_active() const { return name_connected; }

	// Default sizes, font and spacing
	static Pango::FontDescription NAME_FONT;
	static void set_constant_heights ();
	static const double NAME_X_OFFSET;
	static const double GRAB_HANDLE_TOP;
	static const double GRAB_HANDLE_WIDTH;

	/* these are not constant, but vary with the pixel size
	   of the font used to display the item name.
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

	/** Emitted when the name of this item is changed */
	sigc::signal<void,std::string,std::string,void*> NameChanged;

	/** Emiited when the position of this item changes */
	sigc::signal<void,framepos_t,void*> PositionChanged;

	/** Emitted when the position lock of this item is changed */
	sigc::signal<void,bool,void*> PositionLockChanged;

	/** Emitted when the duration of this item changes */
	sigc::signal<void,framecnt_t,void*> DurationChanged;

	/** Emitted when the maximum item duration is changed */
	sigc::signal<void,framecnt_t,void*> MaxDurationChanged;

	/** Emitted when the mionimum item duration is changed */
	sigc::signal<void,framecnt_t,void*> MinDurationChanged;

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

  protected:
	TimeAxisViewItem(const std::string &, ArdourCanvas::Group&, TimeAxisView&, double, Gdk::Color const &,
			 framepos_t, framecnt_t, bool recording = false, bool automation = false, Visibility v = Visibility (0));

	TimeAxisViewItem (const TimeAxisViewItem&);

        void init (ArdourCanvas::Group*, double, Gdk::Color const &, framepos_t, framepos_t, Visibility, bool, bool);

        virtual bool canvas_group_event (GdkEvent*);

	virtual void compute_colors (Gdk::Color const &);
	virtual void set_colors();
	virtual void set_frame_color();
	virtual void set_frame_gradient ();
	void set_trim_handle_colors();

	virtual void reset_width_dependent_items (double);

	static gint idle_remove_this_item(TimeAxisViewItem*, void*);

	/** time axis that this item is on */
	TimeAxisView& trackview;

	/** indicates whether this item is locked to its current position */
	bool position_locked;

	/** position of this item on the timeline */
	framepos_t frame_position;

	/** duration of this item upon the timeline */
	framecnt_t item_duration;

	/** maximum duration that this item can have */
	framecnt_t max_item_duration;

	/** minimum duration that this item can have */
	framecnt_t min_item_duration;

	/** indicates whether the max duration constraint is active */
	bool max_duration_active;

	/** indicates whether the min duration constraint is active */
	bool min_duration_active;

	/** frames per canvas pixel */
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

	/** true if a small vestigial rect should be shown when the item gets very narrow */
	bool show_vestigial;

	uint32_t fill_opacity;
	uint32_t fill_color;
	uint32_t frame_color_r;
	uint32_t frame_color_g;
	uint32_t frame_color_b;
	uint32_t selected_frame_color_r;
	uint32_t selected_frame_color_g;
	uint32_t selected_frame_color_b;
	uint32_t label_color;

	uint32_t handle_color_r;
	uint32_t handle_color_g;
	uint32_t handle_color_b;
	uint32_t lock_handle_color_r;
	uint32_t lock_handle_color_g;
	uint32_t lock_handle_color_b;
	uint32_t last_item_width;
	int name_text_width;
	bool wide_enough_for_name;
	bool high_enough_for_name;
        bool rect_visible;

	ArdourCanvas::Group*      group;
	ArdourCanvas::Rectangle* vestigial_frame;
	ArdourCanvas::Rectangle* frame;
	ArdourCanvas::Text*      name_text;
	ArdourCanvas::Rectangle* name_highlight;

	/* with these two values, if frame_handle_start == 0 then frame_handle_end will also be 0 */
	ArdourCanvas::Rectangle* frame_handle_start; ///< `frame' (fade) handle for the start of the item, or 0
	ArdourCanvas::Rectangle* frame_handle_end; ///< `frame' (fade) handle for the end of the item, or 0

        bool frame_handle_crossing (GdkEvent*, ArdourCanvas::Rectangle*);

	double _height;
	Visibility visibility;
	bool _recregion;
	bool _automation; ///< true if this is an automation region view
	bool _dragging;

private:
        double _width;

	void parameter_changed (std::string);
        void manage_name_highlight ();
        void manage_name_text ();

}; /* class TimeAxisViewItem */

#endif /* __gtk_ardour_time_axis_view_item_h__ */
