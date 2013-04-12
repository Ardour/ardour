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

#include <algorithm>

#include <gtkmm.h>
#include <gtkmm2ext/gtk_ui.h>

#include "marker_time_axis_view.h"
#include "marker_time_axis.h"
#include "marker_view.h"
#include "imageframe_view.h"
#include "imageframe_time_axis.h"
#include "public_editor.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "ardour_ui.h"

#include "i18n.h"

using namespace ARDOUR ;
using namespace Editing;

//---------------------------------------------------------------------------------------//
// Constructor / Desctructor

/**
 * Construct a new MarkerTimeAxisView helper time axis helper
 *
 * @param mta the TimeAxsiView that this objbect is the helper for
 */
MarkerTimeAxisView::MarkerTimeAxisView(MarkerTimeAxis& tv)
	: _trackview (tv)
{
	region_color = _trackview.color();
	stream_base_color = ARDOUR_UI::config()->canvasvar_MarkerTrack.get();

	canvas_group = new ArdourCanvas::Group (*_trackview.canvas_display);

	canvas_rect =  new ArdourCanvas::Rectangle (*canvas_group);
	canvas_rect->property_x1() = 0.0;
	canvas_rect->property_y1() = 0.0;
	canvas_rect->property_x2() = max_framepos;
	canvas_rect->property_y2() = (double)20;
	canvas_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerTrack.get();
	canvas_rect->property_fill_color_rgba() = stream_base_color;

	canvas_rect->signal_event().connect (sigc::bind (sigc::mem_fun (_trackview.editor, &PublicEditor::canvas_marker_time_axis_view_event), canvas_rect, &_trackview));

	_samples_per_pixel = _trackview.editor.get_current_zoom() ;

	_trackview.editor.ZoomChanged.connect (sigc::mem_fun(*this, &MarkerTimeAxisView::reset_samples_per_pixel));
	MarkerView::CatchDeletion.connect (*this, boost::bind (&MarkerTimeAxisView::remove_marker_view, this, _1), gui_context());
}

/**
 * Destructor
 * Reposinsibly for destroying all marker items that may have been added to this time axis view
 *
 */
MarkerTimeAxisView::~MarkerTimeAxisView()
{
	// destroy everything upon this view
	for(MarkerViewList::iterator iter = marker_view_list.begin(); iter != marker_view_list.end(); ++iter)
	{
		MarkerView* mv = (*iter) ;

		MarkerViewList::iterator next = iter ;
		next++ ;
		marker_view_list.erase(iter) ;

		delete mv ;
		mv = 0 ;

		iter = next ;
	}

	delete canvas_rect;
	canvas_rect = 0 ;

	delete canvas_group;
	canvas_group = 0 ;
}


//---------------------------------------------------------------------------------------//
// ui methods & data

/**
 * Sets the height of the time axis view and the item upon it
 *
 * @param height the new height
 */
int
MarkerTimeAxisView::set_height(gdouble h)
{
	if (h < 10.0 || h > 1000.0) {
		return -1;
	}

	canvas_rect->property_y2() = h;

	for (MarkerViewList::iterator i = marker_view_list.begin(); i != marker_view_list.end(); ++i) {
		(*i)->set_y_position_and_height(0, h);
	}

	return 0;
}

/**
 * Sets the position of this view helper on the canvas
 *
 * @param x the x position upon the canvas
 * @param y the y position upon the canvas
 */
int
MarkerTimeAxisView::set_position(gdouble x, gdouble y)
{
	canvas_group->property_x() = x;
	canvas_group->property_y() = y;
	return 0;
}

/**
 * Sets the current frames per pixel.
 * this method tells each item upon the time axis of the change
 *
 * @param fpp the new frames per pixel value
 */
int
MarkerTimeAxisView::set_samples_per_pixel (double fpp)
{
	if (spp < 1.0) {
		return -1;
	}

	_samples_per_pixel = fpp;

	for (MarkerViewList::iterator i = marker_view_list.begin(); i != marker_view_list.end(); ++i) {
		(*i)->set_samples_per_pixel (spp);
	}
	
	return 0;
}

/**
 * Sets the color of the items contained upon this view helper
 *
 * @param color the new base color
 */
void
MarkerTimeAxisView::apply_color(Gdk::Color& color)
{
	region_color = color;

	for (MarkerViewList::iterator i = marker_view_list.begin(); i != marker_view_list.end(); i++)
	{
		(*i)->set_color (region_color) ;
	}
}


//---------------------------------------------------------------------------------------//
// Child MarkerView Accessors/Mutators

/**
 * Adds a marker view to the list of items upon this time axis view helper
 * the new MarkerView is returned
 *
 * @param ifv the ImageFrameView that the new item is marking up
 * @param mark_text the text to be displayed uopn the new marker item
 * @param mark_id the unique id of the new item
 * @param start the position the new item should be placed upon the time line
 * @param duration the duration the new item should be placed upon the timeline
 * @param src the identity of the object that initiated the change
 */
MarkerView*
MarkerTimeAxisView::add_marker_view(ImageFrameView* ifv, std::string mark_type, std::string mark_id, framepos_t start, framecnt_t dur, void* src)
{
	if(ifv->has_marker_view_item(mark_id))
	{
		return(0) ;
	}

	MarkerView* mv = new MarkerView(canvas_group,
		 &_trackview,
		 ifv,
		 _trackview.editor.get_current_zoom(),
		 region_color,
		 mark_type,
		 mark_id,
		 start,
		 dur) ;

	ifv->add_marker_view_item(mv, src) ;
	marker_view_list.push_front(mv) ;

	MarkerViewAdded(mv,src) ; /* EMIT_SIGNAL */

	return(mv) ;
}

/**
 * Returns the named MarkerView or 0 if the named marker does not exist
 *
 * @param item_id the unique id of the item to search for
 * @return the named MarkerView, or 0 if it is not held upon this view
 */
MarkerView*
MarkerTimeAxisView::get_named_marker_view(std::string item_id)
{
	MarkerView* mv =  0 ;

	for(MarkerViewList::iterator i = marker_view_list.begin(); i != marker_view_list.end(); ++i)
	{
		if(((MarkerView*)*i)->get_item_name() == item_id)
		{
			mv = ((MarkerView*)*i) ;
			break ;
		}
	}
	return(mv) ;
}

/**
 * Removes the currently selected MarverView
 * Note that this method actually destroys the MarkerView too.
 * We assume that since we own the object, we are allowed to do this
 *
 * @param src the identity of the object that initiated the change
 * @see add_marker_view
 */
void
MarkerTimeAxisView::remove_selected_marker_view(void* src)
{
	std::string removed ;

	if (selected_time_axis_item)
	{
		MarkerViewList::iterator i ;
		if((i = find (marker_view_list.begin(), marker_view_list.end(), selected_time_axis_item)) != marker_view_list.end())
		{
			marker_view_list.erase(i) ;

			 MarkerViewRemoved(selected_time_axis_item->get_item_name(),src) ; /* EMIT_SIGNAL */

			delete(selected_time_axis_item) ;
			selected_time_axis_item = 0 ;
		}
	}
	else
	{
		//No selected marker view
	}
}

/**
 * Removes and returns the named MarkerView from the list of MarkerView held by this view helper
 *
 * @param item_id the MarkerView unique id to remove
 * @param src the identity of the object that initiated the change
 * @see add_marker_view
 */
MarkerView*
MarkerTimeAxisView::remove_named_marker_view(std::string item_id, void* src)
{
	MarkerView* mv = 0 ;

	MarkerViewList::iterator i = marker_view_list.begin() ;

	for(MarkerViewList::iterator iter = marker_view_list.begin(); iter != marker_view_list.end(); ++iter)
	{
		if(((MarkerView*)*i)->get_item_name() == item_id)
		{
			mv = ((MarkerView*)*i) ;
			marker_view_list.erase(i) ;

			 MarkerViewRemoved(mv->get_item_name(), src) ; /* EMIT_SIGNAL */

			// break from the for loop
			break;
		}
		i++ ;
	}

	return(mv) ;
}

/**
 * Removes mv from the list of MarkerView upon this TimeAxis
 *
 * @param mv the MarkerView to remove
 * @param src the identity of the object that initiated the change
 */
void
MarkerTimeAxisView::remove_marker_view (MarkerView* mv)
{
	ENSURE_GUI_THREAD (*this, &MarkerTimeAxisView::remove_marker_view, mv, src)

	MarkerViewList::iterator i;

	if((i = find (marker_view_list.begin(), marker_view_list.end(), mv)) != marker_view_list.end()) {
		marker_view_list.erase(i) ;

		// Assume this remove happened locally, else use remove_named_marker_time_axis
		// let listeners know that the named MarkerTimeAxis has been removed
		 MarkerViewRemoved(mv->get_item_name(), src) ; /* EMIT_SIGNAL */
	}
}

/**
 * Sets the duration of the selected MarkerView to the specified number of seconds
 *
 * @param sec the duration to set the MArkerView to, in seconds
 */
void
MarkerTimeAxisView::set_marker_duration_sec(double sec)
{
  if(get_selected_time_axis_item() != 0)
  {
	  get_selected_time_axis_item()->set_duration((sec * _trackview.editor.session()->frame_rate()), this);
  }
}


//---------------------------------------------------------------------------------------//
// Selected item methods

/**
 * Sets the currently selected item upon this time axis
 *
 * @param mv the item to set selected
 */
void
MarkerTimeAxisView::set_selected_time_axis_item(MarkerView* mv)
{
	selected_time_axis_item = mv ;
}

/**
 * Clears any selected item upon this time axis
 *
 */
void
MarkerTimeAxisView::clear_selected_time_axis_item()
{
	selected_time_axis_item = 0 ;
}

/**
 * Returnsthe currently selected item upon this time axis
 *
 * @return the currently selected item pon this time axis
 */
MarkerView*
MarkerTimeAxisView::get_selected_time_axis_item()
{
	return(selected_time_axis_item) ;
}




/**
 * convenience method to re-get the samples per unit and tell items upon this view
 *
 */
void
MarkerTimeAxisView::reset_samples_per_pixel ()
{
	set_samples_per_pixel (_trackview.editor.get_current_zoom());
}
