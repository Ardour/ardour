/*
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#include <utility>

#include "pbd/error.h"

#include "ardour/types.h"
#include "ardour/ardour.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gui_thread.h"

#include "canvas/container.h"
#include "canvas/rectangle.h"
#include "canvas/debug.h"
#include "canvas/text.h"
#include "gtkmm2ext/colors.h"

#include "ardour/profile.h"

#include "public_editor.h"
#include "time_axis_view_item.h"
#include "time_axis_view.h"
#include "ui_config.h"
#include "utils.h"
#include "rgb_macros.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Editing;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace Gtkmm2ext;

Pango::FontDescription TimeAxisViewItem::NAME_FONT;
const double TimeAxisViewItem::NAME_X_OFFSET = 15.0;
const double TimeAxisViewItem::GRAB_HANDLE_TOP = 0.0;
const double TimeAxisViewItem::GRAB_HANDLE_WIDTH = 10.0;

int    TimeAxisViewItem::NAME_HEIGHT;
double TimeAxisViewItem::NAME_Y_OFFSET;
double TimeAxisViewItem::NAME_HIGHLIGHT_SIZE;
double TimeAxisViewItem::NAME_HIGHLIGHT_THRESH;

void
TimeAxisViewItem::set_constant_heights ()
{
	NAME_FONT = Pango::FontDescription (UIConfiguration::instance().get_SmallFont());

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

	if (UIConfiguration::instance().get_show_name_highlight()) {
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
	const string & it_name, ArdourCanvas::Item& parent, TimeAxisView& tv, double spu, uint32_t base_color,
	timepos_t const & start, timecnt_t const & duration, bool recording, bool automation, Visibility vis
	)
	: trackview (tv)
	, item_name (it_name)
	, selection_frame (0)
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
	, item_name (other.item_name)
	, selection_frame (0)
	, _height (1.0)
	, _recregion (other._recregion)
	, _automation (other._automation)
	, _dragging (other._dragging)
	, _width (0.0)
{
	/* share the other's parent, but still create a new group */

	ArdourCanvas::Item* parent = other.group->parent();

	_selected = other._selected;

	init (parent, other.samples_per_pixel, other.fill_color, other.time_position,
	      other.item_duration, other.visibility, other.wide_enough_for_name, other.high_enough_for_name);
}

void
TimeAxisViewItem::init (ArdourCanvas::Item* parent, double fpp, uint32_t base_color,
			timepos_t const & start, timecnt_t const & duration, Visibility vis,
			bool wide, bool high)
{
	group = new ArdourCanvas::Container (parent);
	CANVAS_DEBUG_NAME (group, string_compose ("TAVI group for %1", get_item_name()));

	fill_color = base_color;
	fill_color_name = "time axis view item base";
	samples_per_pixel = fpp;
	time_position = start;
	item_duration = duration;
	name_connected = false;
	position_locked = false;
	max_item_duration = timecnt_t::max (item_duration.time_domain());
	min_item_duration = timecnt_t::zero (item_duration.time_domain());
	visibility = vis;
	_sensitive = true;
	name_text_width = 0;
	last_item_width = 0;
	wide_enough_for_name = wide;
	high_enough_for_name = high;

	if (duration == 0) {
		warning << "Time Axis Item Duration == 0" << endl;
	}

	if (visibility & ShowFrame) {
		frame = new ArdourCanvas::Rectangle (group,
		                                     ArdourCanvas::Rect (0.0, 0.0,
		                                                         trackview.editor().sample_to_pixel(duration),
		                                                         trackview.current_height()));

		frame->set_outline_what (ArdourCanvas::Rectangle::What (ArdourCanvas::Rectangle::LEFT|ArdourCanvas::Rectangle::RIGHT));
		frame->show ();

		CANVAS_DEBUG_NAME (frame, string_compose ("frame for %1", get_item_name()));

		if (_recregion) {
			frame->set_outline_color (UIConfiguration::instance().color ("recording rect"));
		} else {
			frame->set_outline_color (UIConfiguration::instance().color ("time axis frame"));
		}
	}

	if (UIConfiguration::instance().get_show_name_highlight() && (visibility & ShowNameHighlight)) {

		/* rectangle size will be set in ::manage_name_highlight() */
		name_highlight = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (name_highlight, string_compose ("name highlight for %1", get_item_name()));
		name_highlight->set_data ("timeaxisviewitem", this);
		name_highlight->set_outline_what (ArdourCanvas::Rectangle::TOP);
		name_highlight->set_outline_color (RGBA_TO_UINT (0,0,0,255)); // this should use a theme color

	} else {
		name_highlight = 0;
	}

	if (visibility & ShowNameText) {
		name_text = new ArdourCanvas::Text (group);
		CANVAS_DEBUG_NAME (name_text, string_compose ("name text for %1", get_item_name()));
		if (UIConfiguration::instance().get_show_name_highlight()) {
			name_text->set_position (ArdourCanvas::Duple (NAME_X_OFFSET, trackview.current_height() - NAME_Y_OFFSET));
		} else {
			name_text->set_position (ArdourCanvas::Duple (NAME_X_OFFSET, NAME_Y_OFFSET));
		}
		name_text->set_font_description (NAME_FONT);
		name_text->set_ignore_events (true);
	} else {
		name_text = 0;
	}

	/* create our grab handles used for trimming/duration etc */
	if (!_recregion && !_automation) {
		double top   = TimeAxisViewItem::GRAB_HANDLE_TOP;
		double width = TimeAxisViewItem::GRAB_HANDLE_WIDTH;

		frame_handle_start = new ArdourCanvas::Rectangle (group, ArdourCanvas::Rect (0.0, top, width, trackview.current_height()));
		CANVAS_DEBUG_NAME (frame_handle_start, "TAVI frame handle start");
		frame_handle_start->set_outline (false);
		frame_handle_start->set_fill (false);
		frame_handle_start->Event.connect (sigc::bind (sigc::mem_fun (*this, &TimeAxisViewItem::frame_handle_crossing), frame_handle_start));

		frame_handle_end = new ArdourCanvas::Rectangle (group, ArdourCanvas::Rect (0.0, top, width, trackview.current_height()));
		CANVAS_DEBUG_NAME (frame_handle_end, "TAVI frame handle end");
		frame_handle_end->set_outline (false);
		frame_handle_end->set_fill (false);
		frame_handle_end->Event.connect (sigc::bind (sigc::mem_fun (*this, &TimeAxisViewItem::frame_handle_crossing), frame_handle_end));
	} else {
		frame_handle_start = frame_handle_end = 0;
	}

	//set_color (base_color);

	//set_duration (item_duration, this);
	//set_position (start, this);

	group->Event.connect (sigc::mem_fun (*this, &TimeAxisViewItem::canvas_group_event));
	//Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&TimeAxisViewItem::parameter_changed, this, _1), gui_context ());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &TimeAxisViewItem::parameter_changed));
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

/**
 * Set the position of this item on the timeline.
 *
 * @param pos the new position
 * @param src the identity of the object that initiated the change
 * @return true on success
 */

bool
TimeAxisViewItem::set_position(timepos_t const & pos, void* src, double* delta)
{
	if (position_locked) {
		return false;
	}

	position = pos;

	double new_unit_pos = trackview.editor().time_to_pixel (time_position);

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
	PositionChanged (time_position, src); /* EMIT_SIGNAL */

	return true;
}

/** @return position of this item on the timeline */
timepos_t
TimeAxisViewItem::get_position() const
{
	return time_position;
}

/**
 * Set the duration of this item.
 *
 * @param dur the new duration of this item
 * @param src the identity of the object that initiated the change
 * @return true on success
 */

bool
TimeAxisViewItem::set_duration (timecnt_t const & dur, void* src)
{
	if ((dur > max_item_duration) || (dur < min_item_duration)) {
		warning << string_compose (
				P_("new duration %1 frame is out of bounds for %2", "new duration of %1 samples is out of bounds for %2", dur),
				get_item_name(), dur)
			<< endmsg;
		return false;
	}

	if (dur == 0) {
		group->hide();
	}

	item_duration = dur;

	double end_pixel = trackview.editor().time_to_pixel (time_position + dur);
	double first_pixel = trackview.editor().time_to_pixel (time_position);

	reset_width_dependent_items (end_pixel - first_pixel);

	DurationChanged (dur, src); /* EMIT_SIGNAL */
	return true;
}

/** @return duration of this item */
timecnt_t
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
TimeAxisViewItem::set_max_duration(samplecnt_t dur, void* src)
{
	max_item_duration = dur;
	MaxDurationChanged(max_item_duration, src); /* EMIT_SIGNAL */
}

/** @return the maximum duration that this item may have */
timecnt_t
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
TimeAxisViewItem::set_min_duration(samplecnt_t dur, void* src)
{
	min_item_duration = dur;
	MinDurationChanged(max_item_duration, src); /* EMIT_SIGNAL */
}

/** @return the minimum duration that this item mey have */
timecnt_t
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
	if (_selected == yn) {
		return;
	}

	Selectable::set_selected (yn);
	set_frame_color ();
	set_name_text_color ();

	if (_selected && frame) {
		if (!selection_frame) {
			selection_frame = new ArdourCanvas::Rectangle (group);
			selection_frame->set_fill (false);
			selection_frame->set_outline_color (UIConfiguration::instance().color ("selected time axis frame"));
			selection_frame->set_ignore_events (true);
		}
		selection_frame->set (frame->get().shrink (1.0));
		selection_frame->show ();
	} else {
		if (selection_frame) {
			selection_frame->hide ();
		}
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
	manage_name_text ();
	manage_name_highlight ();
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
		if (UIConfiguration::instance().get_show_name_highlight()) {
			name_text->set_y_position (height - NAME_Y_OFFSET);
		} else {
			name_text->set_y_position (NAME_Y_OFFSET);
		}
	}

	if (frame) {

		frame->set_y0 (0.0);
		frame->set_y1 (height);

		if (frame_handle_start) {
			frame_handle_start->set_y1 (height);
			frame_handle_end->set_y1 (height);
		}

		if (selection_frame) {
			selection_frame->set (frame->get().shrink (1.0));
		}
	}
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
		// name_highlight->set_x_position (1.0);
		name_highlight->set (ArdourCanvas::Rect (0.0, (double) _height - NAME_HIGHLIGHT_SIZE,  _width - 2.0, _height));

	} else {
		name_highlight->hide();
	}

	manage_name_text ();
}

void
TimeAxisViewItem::set_color (uint32_t base_color)
{
	fill_color = base_color;
	set_colors ();
}

ArdourCanvas::Item*
TimeAxisViewItem::get_canvas_frame()
{
	return frame;
}

ArdourCanvas::Item*
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
 * Convenience method to set the various canvas item colors
 */
void
TimeAxisViewItem::set_colors()
{
	set_frame_color ();

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


	uint32_t f;

	if (UIConfiguration::instance().get_show_name_highlight()) {
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

	name_text->set_color (contrasting_text_color (f));
}

Gtkmm2ext::Color
TimeAxisViewItem::get_fill_color () const
{
	const std::string mod_name = (_dragging ? "dragging region" : fill_color_name);

	if (_selected) {
		return UIConfiguration::instance().color ("selected region base");
	} else if (_recregion) {
		return UIConfiguration::instance().color ("recording rect");
	} else if ((!UIConfiguration::instance().get_show_name_highlight() || high_enough_for_name) &&
	           !UIConfiguration::instance().get_color_regions_using_track_color()) {
		return UIConfiguration::instance().color_mod (fill_color_name, mod_name);
	}
	return UIConfiguration::instance().color_mod (fill_color, mod_name);
}

/**
 * Sets the frame color depending on whether this item is selected
 */
void
TimeAxisViewItem::set_frame_color()
{
	if (!frame) {
		return;
	}

	frame->set_fill_color (get_fill_color());
	set_frame_gradient ();

	if (!_recregion) {
		frame->set_outline_color (UIConfiguration::instance().color ("time axis frame"));
	}
}

void
TimeAxisViewItem::set_frame_gradient ()
{
	if (UIConfiguration::instance().get_timeline_item_gradient_depth() == 0.0) {
		frame->set_gradient (ArdourCanvas::Fill::StopList (), 0);
		return;
	}

	ArdourCanvas::Fill::StopList stops;
	double r, g, b, a;
	double h, s, v;
	Color f (get_fill_color());

	/* need to get alpha value */
	color_to_rgba (f, r, g, b, a);

	stops.push_back (std::make_pair (0.0, f));

	/* now a darker version */

	color_to_hsv (f, h, s, v);

	v = min (1.0, v * (1.0 - UIConfiguration::instance().get_timeline_item_gradient_depth()));

	Color darker = hsva_to_color (h, s, v, a);
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
			frame_handle_start->set_fill_color (UIConfiguration::instance().get_TrimHandleLocked());
			frame_handle_end->set_fill_color (UIConfiguration::instance().get_TrimHandleLocked());
		} else {
			frame_handle_start->set_fill_color (UIConfiguration::instance().get_TrimHandle());
			frame_handle_end->set_fill_color (UIConfiguration::instance().get_TrimHandle());
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
		if (trackview.editor().effective_mouse_mode() == Editing::MouseObject) {
			/* Never set this to be visible in other modes.  Note, however,
			   that we do need to undo visibility (LEAVE_NOTIFY case above) no
			   matter what the mode is. */
			item->set_fill (true);
		}
		break;
	default:
		break;
	}
	return false;
}

/** @return the samples per pixel */
double
TimeAxisViewItem::get_samples_per_pixel () const
{
	return samples_per_pixel;
}

/** Set the samples per pixel of this item.
 *  This item is used to determine the relative visual size and position of this item
 *  based upon its duration and start value.
 *
 *  @param fpp the new samples per pixel
 */
void
TimeAxisViewItem::set_samples_per_pixel (double fpp)
{
	samples_per_pixel = fpp;
	set_position (this->get_position(), this);

	double end_pixel = trackview.editor().time_to_pixel (timeposition + get_duration());
	double first_pixel = trackview.editor().time_to_pixel (time_position);

	reset_width_dependent_items (end_pixel - first_pixel);
}

void
TimeAxisViewItem::reset_width_dependent_items (double pixel_width)
{
	_width = pixel_width;

	manage_name_highlight ();
	manage_name_text ();

	if (pixel_width < 2.0) {

		if (frame) {
			frame->set_outline (false);
			frame->set_x1 (std::max(1.0, pixel_width));
		}

		if (frame_handle_start) {
			frame_handle_start->hide();
			frame_handle_end->hide();
		}

	} else {
		if (frame) {
			frame->set_outline (true);
			/* Note: x0 is always zero - the position is defined by
			 * the position of the group, not the frame.
			 */
			frame->set_x1 (pixel_width);

			if (selection_frame) {
				selection_frame->set (frame->get().shrink (1.0));
			}
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
				frame_handle_end->set_x0 (pixel_width - (TimeAxisViewItem::GRAB_HANDLE_WIDTH));
				frame_handle_end->set_x1 (pixel_width);
				frame_handle_end->show();

				frame_handle_start->raise_to_top ();
				frame_handle_end->raise_to_top ();
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

	if (!(visibility & ShowNameText) || (!wide_enough_for_name || !high_enough_for_name)) {
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

void
TimeAxisViewItem::drag_start ()
{
	_dragging = true;
	set_frame_color ();
}

void
TimeAxisViewItem::drag_end ()
{
	_dragging = false;
	set_frame_color ();
}
