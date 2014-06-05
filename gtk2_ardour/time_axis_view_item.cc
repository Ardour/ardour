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

#include <utility>

#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "ardour/types.h"
#include "ardour/ardour.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gui_thread.h"

#include "canvas/group.h"
#include "canvas/rectangle.h"
#include "canvas/debug.h"
#include "canvas/drag_handle.h"
#include "canvas/text.h"
#include "canvas/utils.h"

#include "ardour/profile.h"

#include "ardour_ui.h"
/*
 * ardour_ui.h was moved up in the include list
 * due to a conflicting definition of 'Rect' between
 * Apple's MacTypes.h file and GTK
 */

#include "public_editor.h"
#include "time_axis_view_item.h"
#include "time_axis_view.h"
#include "utils.h"
#include "rgb_macros.h"

#include "i18n.h"

using namespace std;
using namespace Editing;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;
using namespace Gtkmm2ext;

Pango::FontDescription TimeAxisViewItem::NAME_FONT;
const double TimeAxisViewItem::NAME_X_OFFSET = 15.0;
const double TimeAxisViewItem::GRAB_HANDLE_TOP = 0.0;
const double TimeAxisViewItem::GRAB_HANDLE_WIDTH = 10.0;
const double TimeAxisViewItem::RIGHT_EDGE_SHIFT = 1.0;

int    TimeAxisViewItem::NAME_HEIGHT;
double TimeAxisViewItem::NAME_Y_OFFSET;
double TimeAxisViewItem::NAME_HIGHLIGHT_SIZE;
double TimeAxisViewItem::NAME_HIGHLIGHT_THRESH;

void
TimeAxisViewItem::set_constant_heights ()
{
        NAME_FONT = get_font_for_style (X_("TimeAxisViewItemName"));

        Gtk::Window win;
        Gtk::Label foo;
        win.add (foo);

        Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (X_("Hg")); /* ascender + descender */
        int width = 0;
        int height = 0;

        layout->set_font_description (NAME_FONT);
        get_pixel_size (layout, width, height);

        layout = foo.create_pango_layout (X_("H")); /* just the ascender */

        NAME_HEIGHT = height;

	/* Config->get_show_name_highlight) == true: 
	        Y_OFFSET is measured from bottom of the time axis view item.
	   Config->get_show_name_highlight) == false: 
	        Y_OFFSET is measured from the top of the time axis view item.
	*/

	if (Config->get_show_name_highlight()) {
		NAME_Y_OFFSET = height + 1;
		NAME_HIGHLIGHT_SIZE = height + 2;
	} else {
		NAME_Y_OFFSET = 3;
		NAME_HIGHLIGHT_SIZE = 0;
	}
        NAME_HIGHLIGHT_THRESH = NAME_HIGHLIGHT_SIZE * 3;
}

/**
 * Construct a new TimeAxisViewItem.
 *
 * @param it_name the unique name of this item
 * @param parent the parent canvas group
 * @param tv the TimeAxisView we are going to be added to
 * @param spu samples per unit
 * @param base_color
 * @param start the start point of this item
 * @param duration the duration of this item
 * @param recording true if this is a recording region view
 * @param automation true if this is an automation region view
 */
TimeAxisViewItem::TimeAxisViewItem(
	const string & it_name, ArdourCanvas::Group& parent, TimeAxisView& tv, double spu, Gdk::Color const & base_color,
	framepos_t start, framecnt_t duration, bool recording, bool automation, Visibility vis
	)
	: trackview (tv)
	, frame_position (-1)
	, item_name (it_name)
	, _height (1.0)
	, _recregion (recording)
	, _automation (automation)
	, _dragging (false)
	, _width (0.0)
{
	init (&parent, spu, base_color, start, duration, vis, true, true);
}

TimeAxisViewItem::TimeAxisViewItem (const TimeAxisViewItem& other)
	: trackable (other)
	, Selectable (other)
	, PBD::ScopedConnectionList()
	, trackview (other.trackview)
	, frame_position (-1)
	, item_name (other.item_name)
	, _height (1.0)
	, _recregion (other._recregion)
	, _automation (other._automation)
	, _dragging (other._dragging)
	, _width (0.0)
{

	Gdk::Color c;
	int r,g,b,a;

	UINT_TO_RGBA (other.fill_color, &r, &g, &b, &a);
	c.set_rgb_p (r/255.0, g/255.0, b/255.0);

	/* share the other's parent, but still create a new group */

	ArdourCanvas::Group* parent = other.group->parent();
	
	_selected = other._selected;
	
	init (parent, other.samples_per_pixel, c, other.frame_position,
	      other.item_duration, other.visibility, other.wide_enough_for_name, other.high_enough_for_name);
}

void
TimeAxisViewItem::init (ArdourCanvas::Group* parent, double fpp, Gdk::Color const & base_color, 
			framepos_t start, framepos_t duration, Visibility vis, 
			bool wide, bool high)
{
	group = new ArdourCanvas::Group (parent);
	CANVAS_DEBUG_NAME (group, string_compose ("TAVI group for %1", get_item_name()));
	group->Event.connect (sigc::mem_fun (*this, &TimeAxisViewItem::canvas_group_event));

	samples_per_pixel = fpp;
	frame_position = start;
	item_duration = duration;
	name_connected = false;
	fill_opacity = 60;
	position_locked = false;
	max_item_duration = ARDOUR::max_framepos;
	min_item_duration = 0;
	show_vestigial = true;
	visibility = vis;
	_sensitive = true;
	name_text_width = 0;
	last_item_width = 0;
	wide_enough_for_name = wide;
	high_enough_for_name = high;
        rect_visible = true;

	if (duration == 0) {
		warning << "Time Axis Item Duration == 0" << endl;
	}

	vestigial_frame = new ArdourCanvas::Rectangle (group, ArdourCanvas::Rect (0.0, 1.0, 2.0, trackview.current_height()));
	CANVAS_DEBUG_NAME (vestigial_frame, string_compose ("vestigial frame for %1", get_item_name()));
	vestigial_frame->hide ();
	vestigial_frame->set_outline_color (ARDOUR_UI::config()->get_canvasvar_VestigialFrame());
	vestigial_frame->set_fill_color (ARDOUR_UI::config()->get_canvasvar_VestigialFrame());

	if (visibility & ShowFrame) {
		frame = new ArdourCanvas::Rectangle (group, 
						     ArdourCanvas::Rect (0.0, 0.0, 
									 trackview.editor().sample_to_pixel(duration) + RIGHT_EDGE_SHIFT, 
									 trackview.current_height() - 1.0));

		CANVAS_DEBUG_NAME (frame, string_compose ("frame for %1", get_item_name()));
		
		if (Config->get_show_name_highlight()) {
			frame->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::LEFT|ArdourCanvas::Rectangle::RIGHT));
		} else {
			frame->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::LEFT|ArdourCanvas::Rectangle::RIGHT|ArdourCanvas::Rectangle::BOTTOM));
		}

		if (_recregion) {
			frame->set_outline_color (ARDOUR_UI::config()->get_canvasvar_RecordingRect());
		} else {
			frame->set_outline_color (ARDOUR_UI::config()->get_canvasvar_TimeAxisFrame());
		}

	} else {

		frame = 0;
	}
	
	if (Config->get_show_name_highlight() && (visibility & ShowNameHighlight)) {

		double width;
		double start;

		if (visibility & FullWidthNameHighlight) {
			start = 0.0;
			width = trackview.editor().sample_to_pixel(item_duration) + RIGHT_EDGE_SHIFT;
		} else {
			start = 1.0;
			width = trackview.editor().sample_to_pixel(item_duration) - 2.0 + RIGHT_EDGE_SHIFT;
		}

		name_highlight = new ArdourCanvas::Rectangle (group, 
							      ArdourCanvas::Rect (start, 
										  trackview.current_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE, 
										  width - 2.0 + RIGHT_EDGE_SHIFT,
										  trackview.current_height() - 1.0));
		CANVAS_DEBUG_NAME (name_highlight, string_compose ("name highlight for %1", get_item_name()));
		name_highlight->set_data ("timeaxisviewitem", this);
                name_highlight->set_outline_what (ArdourCanvas::Rectangle::TOP);
		name_highlight->set_outline_color (RGBA_TO_UINT (0,0,0,255));

	} else {
		name_highlight = 0;
	}

	if (visibility & ShowNameText) {
		name_text = new ArdourCanvas::Text (group);
		CANVAS_DEBUG_NAME (name_text, string_compose ("name text for %1", get_item_name()));
		if (Config->get_show_name_highlight()) {
			name_text->set_position (ArdourCanvas::Duple (NAME_X_OFFSET, trackview.current_height() - NAME_Y_OFFSET));
		} else {
			name_text->set_position (ArdourCanvas::Duple (NAME_X_OFFSET, NAME_Y_OFFSET));
		}
		name_text->set_font_description (NAME_FONT);
	} else {
		name_text = 0;
	}

	/* create our grab handles used for trimming/duration etc */
	if (!_recregion && !_automation) {
		double top   = TimeAxisViewItem::GRAB_HANDLE_TOP;
		double width = TimeAxisViewItem::GRAB_HANDLE_WIDTH;

		frame_handle_start = new ArdourCanvas::DragHandle (group, ArdourCanvas::Rect (0.0, top, width, trackview.current_height()), true);
		CANVAS_DEBUG_NAME (frame_handle_start, "TAVI frame handle start");
		frame_handle_start->set_outline (false);
		frame_handle_start->set_fill (false);
		frame_handle_start->Event.connect (sigc::bind (sigc::mem_fun (*this, &TimeAxisViewItem::frame_handle_crossing), frame_handle_start));

		frame_handle_end = new ArdourCanvas::DragHandle (group, ArdourCanvas::Rect (0.0, top, width, trackview.current_height()), false);
		CANVAS_DEBUG_NAME (frame_handle_end, "TAVI frame handle end");
		frame_handle_end->set_outline (false);
		frame_handle_end->set_fill (false);
		frame_handle_end->Event.connect (sigc::bind (sigc::mem_fun (*this, &TimeAxisViewItem::frame_handle_crossing), frame_handle_end));
	} else {
		frame_handle_start = frame_handle_end = 0;
	}

	set_color (base_color);

	set_duration (item_duration, this);
	set_position (start, this);

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&TimeAxisViewItem::parameter_changed, this, _1), gui_context ());
	ARDOUR_UI::config()->ParameterChanged.connect (sigc::mem_fun (*this, &TimeAxisViewItem::parameter_changed));
}

TimeAxisViewItem::~TimeAxisViewItem()
{
	delete group;
}

bool
TimeAxisViewItem::canvas_group_event (GdkEvent* /*ev*/)
{
	return false;
}

void
TimeAxisViewItem::hide_rect ()
{
        rect_visible = false;
        set_frame_color ();

        if (name_highlight) {
                name_highlight->set_outline_what (ArdourCanvas::Rectangle::What (0));
                name_highlight->set_fill_color (UINT_RGBA_CHANGE_A (fill_color, 64));
        }
}

void
TimeAxisViewItem::show_rect ()
{
        rect_visible = true;
        set_frame_color ();

        if (name_highlight) {
                name_highlight->set_outline_what (ArdourCanvas::Rectangle::TOP);
                name_highlight->set_fill_color (fill_color);
        }
}

/**
 * Set the position of this item on the timeline.
 *
 * @param pos the new position
 * @param src the identity of the object that initiated the change
 * @return true on success
 */

bool
TimeAxisViewItem::set_position(framepos_t pos, void* src, double* delta)
{
	if (position_locked) {
		return false;
	}

	frame_position = pos;

	double new_unit_pos = trackview.editor().sample_to_pixel (pos);

	if (delta) {
		(*delta) = new_unit_pos - group->position().x;
		if (*delta == 0.0) {
			return true;
		}
	} else {
		if (new_unit_pos == group->position().x) {
			return true;
		}
	}

	group->set_x_position (new_unit_pos);

	PositionChanged (frame_position, src); /* EMIT_SIGNAL */

	return true;
}

/** @return position of this item on the timeline */
framepos_t
TimeAxisViewItem::get_position() const
{
	return frame_position;
}

/**
 * Set the duration of this item.
 *
 * @param dur the new duration of this item
 * @param src the identity of the object that initiated the change
 * @return true on success
 */

bool
TimeAxisViewItem::set_duration (framecnt_t dur, void* src)
{
	if ((dur > max_item_duration) || (dur < min_item_duration)) {
		warning << string_compose (
				P_("new duration %1 frame is out of bounds for %2", "new duration of %1 frames is out of bounds for %2", dur),
				get_item_name(), dur)
			<< endmsg;
		return false;
	}

	if (dur == 0) {
		group->hide();
	}

	item_duration = dur;

	reset_width_dependent_items (trackview.editor().sample_to_pixel (dur));

	DurationChanged (dur, src); /* EMIT_SIGNAL */
	return true;
}

/** @return duration of this item */
framepos_t
TimeAxisViewItem::get_duration() const
{
	return item_duration;
}

/**
 * Set the maximum duration that this item can have.
 *
 * @param dur the new maximum duration
 * @param src the identity of the object that initiated the change
 */
void
TimeAxisViewItem::set_max_duration(framecnt_t dur, void* src)
{
	max_item_duration = dur;
	MaxDurationChanged(max_item_duration, src); /* EMIT_SIGNAL */
}

/** @return the maximum duration that this item may have */
framecnt_t
TimeAxisViewItem::get_max_duration() const
{
	return max_item_duration;
}

/**
 * Set the minimum duration that this item may have.
 *
 * @param the minimum duration that this item may be set to
 * @param src the identity of the object that initiated the change
 */
void
TimeAxisViewItem::set_min_duration(framecnt_t dur, void* src)
{
	min_item_duration = dur;
	MinDurationChanged(max_item_duration, src); /* EMIT_SIGNAL */
}

/** @return the minimum duration that this item mey have */
framecnt_t
TimeAxisViewItem::get_min_duration() const
{
	return min_item_duration;
}

/**
 * Set whether this item is locked to its current position.
 * Locked items cannot be moved until the item is unlocked again.
 *
 * @param yn true to lock this item to its current position
 * @param src the identity of the object that initiated the change
 */
void
TimeAxisViewItem::set_position_locked(bool yn, void* src)
{
	position_locked = yn;
	set_trim_handle_colors();
	PositionLockChanged (position_locked, src); /* EMIT_SIGNAL */
}

/** @return true if this item is locked to its current position */
bool
TimeAxisViewItem::get_position_locked() const
{
	return position_locked;
}

/**
 * Set whether the maximum duration constraint is active.
 *
 * @param active set true to enforce the max duration constraint
 * @param src the identity of the object that initiated the change
 */
void
TimeAxisViewItem::set_max_duration_active (bool active, void* /*src*/)
{
	max_duration_active = active;
}

/** @return true if the maximum duration constraint is active */
bool
TimeAxisViewItem::get_max_duration_active() const
{
	return max_duration_active;
}

/**
 * Set whether the minimum duration constraint is active.
 *
 * @param active set true to enforce the min duration constraint
 * @param src the identity of the object that initiated the change
 */

void
TimeAxisViewItem::set_min_duration_active (bool active, void* /*src*/)
{
	min_duration_active = active;
}

/** @return true if the maximum duration constraint is active */
bool
TimeAxisViewItem::get_min_duration_active() const
{
	return min_duration_active;
}

/**
 * Set the name of this item.
 *
 * @param new_name the new name of this item
 * @param src the identity of the object that initiated the change
 */

void
TimeAxisViewItem::set_item_name(std::string new_name, void* src)
{
	if (new_name != item_name) {
		std::string temp_name = item_name;
		item_name = new_name;
		NameChanged (item_name, temp_name, src); /* EMIT_SIGNAL */
	}
}

/** @return the name of this item */
std::string
TimeAxisViewItem::get_item_name() const
{
	return item_name;
}

/**
 * Set selection status.
 *
 * @param yn true if this item is currently selected
 */
void
TimeAxisViewItem::set_selected(bool yn)
{
	if (_selected != yn) {
		Selectable::set_selected (yn);
		set_frame_color ();
	}
}

/** @return the TimeAxisView that this item is on */
TimeAxisView&
TimeAxisViewItem::get_time_axis_view () const
{
	return trackview;
}

/**
 * Set the displayed item text.
 * This item is the visual text name displayed on the canvas item, this can be different to the name of the item.
 *
 * @param new_name the new name text to display
 */

void
TimeAxisViewItem::set_name_text(const string& new_name)
{
	if (!name_text) {
		return;
	}

	name_text_width = pixel_width (new_name, NAME_FONT) + 2;
	name_text->set (new_name);

}

/**
 * Set the height of this item.
 *
 * @param h new height
 */
void
TimeAxisViewItem::set_height (double height)
{
        _height = height;

	manage_name_highlight ();

	if (visibility & ShowNameText) {
		if (Config->get_show_name_highlight()) {
			name_text->set_y_position (height - NAME_Y_OFFSET); 
		} else {
			name_text->set_y_position (NAME_Y_OFFSET); 
		}
	}

	if (frame) {
		frame->set_y1 (height);
		if (frame_handle_start) {
			frame_handle_start->set_y1 (height);
			frame_handle_end->set_y1 (height);
		}
	}

	vestigial_frame->set_y1 (height - 1.0);

	set_colors ();
}

void
TimeAxisViewItem::manage_name_highlight ()
{
	if (!name_highlight) {
		return;
	}

	if (_height < NAME_HIGHLIGHT_THRESH) {
		high_enough_for_name = false;
	} else {
		high_enough_for_name = true;
	}

	if (_width < 2.0) {
		wide_enough_for_name = false;
	} else {
		wide_enough_for_name = true;
	}

	if (name_highlight && wide_enough_for_name && high_enough_for_name) {

		name_highlight->show();
		name_highlight->set (ArdourCanvas::Rect (0.0, (double) _height - NAME_HIGHLIGHT_SIZE,  _width+RIGHT_EDGE_SHIFT, (double) _height - 1.0));
			
	} else {
		name_highlight->hide();
	}

	manage_name_text ();
}

void
TimeAxisViewItem::set_color (Gdk::Color const & base_color)
{
	compute_colors (base_color);
	set_colors ();
}

ArdourCanvas::Item*
TimeAxisViewItem::get_canvas_frame()
{
	return frame;
}

ArdourCanvas::Group*
TimeAxisViewItem::get_canvas_group()
{
	return group;
}

ArdourCanvas::Item*
TimeAxisViewItem::get_name_highlight()
{
	return name_highlight;
}

/**
 * Calculate some contrasting color for displaying various parts of this item, based upon the base color.
 *
 * @param color the base color of the item
 */
void
TimeAxisViewItem::compute_colors (Gdk::Color const & base_color)
{
	unsigned char r,g,b;

	/* FILL: change opacity to a fixed value */

	r = base_color.get_red()/256;
	g = base_color.get_green()/256;
	b = base_color.get_blue()/256;
	fill_color = RGBA_TO_UINT(r,g,b,160);
}

/**
 * Convenience method to set the various canvas item colors
 */
void
TimeAxisViewItem::set_colors()
{
	set_frame_color();

	if (name_highlight) {
                name_highlight->set_fill_color (fill_color);
	}

	set_name_text_color ();
	set_trim_handle_colors();
}

void
TimeAxisViewItem::set_name_text_color ()
{
	if (!name_text) {
		return;
	}
	
	double r, g, b, a;

	const double black_r = 0.0;
	const double black_g = 0.0;
	const double black_b = 0.0;

	const double white_r = 1.0;
	const double white_g = 1.0;
	const double white_b = 1.0;

	uint32_t f;
	
	if (Config->get_show_name_highlight()) {
		/* name text will always be on top of name highlight, which
		   will always use our fill color.
		*/
		f = fill_color;
	} else {
		/* name text will be on top of the item, whose color
		   may vary depending on various conditions.
		*/
		f = get_fill_color ();
	}

	ArdourCanvas::color_to_rgba (f, r, g, b, a);

	/* Use W3C contrast guideline calculation */

	double white_contrast = (max (r, white_r) - min (r, white_r)) +
		(max (g, white_g) - min (g, white_g)) + 
		(max (b, white_b) - min (b, white_b));

	double black_contrast = (max (r, black_r) - min (r, black_r)) +
		(max (g, black_g) - min (g, black_g)) + 
		(max (b, black_b) - min (b, black_b));

	if (white_contrast > black_contrast) {		
		/* use white */
		name_text->set_color (ArdourCanvas::rgba_to_color (1.0, 1.0, 1.0, 1.0));
	} else {
		/* use black */
		name_text->set_color (ArdourCanvas::rgba_to_color (0.0, 0.0, 0.0, 1.0));
	}
}

uint32_t
TimeAxisViewItem::get_fill_color () const
{
        uint32_t f = 0;

	if (_selected) {

                f = ARDOUR_UI::config()->get_canvasvar_SelectedFrameBase();

	} else {

		if (_recregion) {
			f = ARDOUR_UI::config()->get_canvasvar_RecordingRect();
		} else {

			if (high_enough_for_name && !ARDOUR_UI::config()->get_color_regions_using_track_color()) {
				f = ARDOUR_UI::config()->get_canvasvar_FrameBase();
			} else {
				f = fill_color;
			}
		}
	}

	return f;
}

/**
 * Sets the frame color depending on whether this item is selected
 */
void
TimeAxisViewItem::set_frame_color()
{
        uint32_t f = 0;

	if (!frame) {
		return;
	}

	f = get_fill_color ();

	if (fill_opacity) {
		f = UINT_RGBA_CHANGE_A (f, fill_opacity);
	}
	
	if (!rect_visible) {
		f = UINT_RGBA_CHANGE_A (f, 0);
	}

        frame->set_fill_color (f);
	set_frame_gradient ();

        if (!_recregion) {
                if (_selected) {
                        f = ARDOUR_UI::config()->get_canvasvar_SelectedTimeAxisFrame();
                } else {
                        f = ARDOUR_UI::config()->get_canvasvar_TimeAxisFrame();
                }

                if (!rect_visible) {
                        f = UINT_RGBA_CHANGE_A (f, 64);
                }

                frame->set_outline_color (f);
        }
}

void
TimeAxisViewItem::set_frame_gradient ()
{
	if (ARDOUR_UI::config()->get_timeline_item_gradient_depth() == 0.0) {
		frame->set_gradient (ArdourCanvas::Fill::StopList (), 0);
		return;
	}
		
	ArdourCanvas::Fill::StopList stops;
	double r, g, b, a;
	double h, s, v;
	ArdourCanvas::Color f (get_fill_color());

	/* need to get alpha value */
	ArdourCanvas::color_to_rgba (f, r, g, b, a);
	
	stops.push_back (std::make_pair (0.0, f));
	
	/* now a darker version */
	
	ArdourCanvas::color_to_hsv (f, h, s, v);

	v = min (1.0, v * (1.0 - ARDOUR_UI::config()->get_timeline_item_gradient_depth()));
	
	ArdourCanvas::Color darker = ArdourCanvas::hsv_to_color (h, s, v, a);
	stops.push_back (std::make_pair (1.0, darker));
	
	frame->set_gradient (stops, true);
}

/**
 * Set the colors of the start and end trim handle depending on object state
 */
void
TimeAxisViewItem::set_trim_handle_colors()
{
#if 1
	/* Leave them transparent for now */
	if (frame_handle_start) {
		frame_handle_start->set_fill_color (0x00000000);
		frame_handle_end->set_fill_color (0x00000000);
	}
#else
	if (frame_handle_start) {
		if (position_locked) {
			frame_handle_start->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TrimHandleLocked());
			frame_handle_end->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TrimHandleLocked());
		} else {
			frame_handle_start->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TrimHandle());
			frame_handle_end->set_fill_color (ARDOUR_UI::config()->get_canvasvar_TrimHandle());
		}
	}
#endif
}

bool
TimeAxisViewItem::frame_handle_crossing (GdkEvent* ev, ArdourCanvas::Rectangle* item)
{
	switch (ev->type) {
	case GDK_LEAVE_NOTIFY:
		/* always hide the handle whenever we leave, no matter what mode */
		item->set_fill (false);
		break;
	case GDK_ENTER_NOTIFY:
		if (trackview.editor().effective_mouse_mode() == Editing::MouseObject &&
		    !trackview.editor().internal_editing()) {
			/* never set this to be visible in internal
			   edit mode. Note, however, that we do need to
			   undo visibility (LEAVE_NOTIFY case above) no
			   matter what the mode is.
			*/
			item->set_fill (true);
		}
		break;
	default:
		break;
	}
	return false;
}

/** @return the frames per pixel */
double
TimeAxisViewItem::get_samples_per_pixel () const
{
	return samples_per_pixel;
}

/** Set the frames per pixel of this item.
 *  This item is used to determine the relative visual size and position of this item
 *  based upon its duration and start value.
 *
 *  @param fpp the new frames per pixel
 */
void
TimeAxisViewItem::set_samples_per_pixel (double fpp)
{
	samples_per_pixel = fpp;
	set_position (this->get_position(), this);
	reset_width_dependent_items ((double) get_duration() / samples_per_pixel);
}

void
TimeAxisViewItem::reset_width_dependent_items (double pixel_width)
{
	_width = pixel_width;

	manage_name_highlight ();

	if (pixel_width < 2.0) {

		if (show_vestigial) {
			vestigial_frame->show();
		}

		if (frame) {
			frame->hide();
		}

		if (frame_handle_start) {
			frame_handle_start->hide();
			frame_handle_end->hide();
		}

	} else {
		vestigial_frame->hide();

		if (frame) {
			frame->show();
			frame->set_x1 (pixel_width + RIGHT_EDGE_SHIFT);
		}

		if (frame_handle_start) {
			if (pixel_width < (3 * TimeAxisViewItem::GRAB_HANDLE_WIDTH)) {
				/*
				 * there's less than GRAB_HANDLE_WIDTH of the region between 
				 * the right-hand end of frame_handle_start and the left-hand
				 * end of frame_handle_end, so disable the handles
				 */

				frame_handle_start->hide();
				frame_handle_end->hide();
			} else {
				frame_handle_start->show();
				frame_handle_end->set_x0 (pixel_width + RIGHT_EDGE_SHIFT - (TimeAxisViewItem::GRAB_HANDLE_WIDTH));
				frame_handle_end->set_x1 (pixel_width + RIGHT_EDGE_SHIFT);
				frame_handle_end->show();
			}
		}
	}
}

void
TimeAxisViewItem::manage_name_text ()
{
	int visible_name_width;

	if (!name_text) {
		return;
	}

	if (!wide_enough_for_name || !high_enough_for_name) {
		name_text->hide ();
		return;
	}
		
	if (name_text->text().empty()) {
		name_text->hide ();
	}

	visible_name_width = name_text_width;

	if (visible_name_width > _width - NAME_X_OFFSET) {
		visible_name_width = _width - NAME_X_OFFSET;
	}

	if (visible_name_width < 1) {
		name_text->hide ();
	} else {
		name_text->clamp_width (visible_name_width);
		name_text->show ();
	}
}

/**
 * Callback used to remove this time axis item during the gtk idle loop.
 * This is used to avoid deleting the obejct while inside the remove_this_item
 * method.
 *
 * @param item the TimeAxisViewItem to remove.
 * @param src the identity of the object that initiated the change.
 */
gint
TimeAxisViewItem::idle_remove_this_item(TimeAxisViewItem* item, void* src)
{
	item->ItemRemoved (item->get_item_name(), src); /* EMIT_SIGNAL */
	delete item;
	item = 0;
	return false;
}

void
TimeAxisViewItem::set_y (double y)
{
	group->set_y_position (y);
}

void
TimeAxisViewItem::parameter_changed (string p)
{
	if (p == "color-regions-using-track-color") {
		set_colors ();
	} else if (p == "timeline-item-gradient-depth") {
		set_frame_gradient ();
	}
}
