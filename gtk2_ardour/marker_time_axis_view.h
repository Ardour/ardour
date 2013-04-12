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

#ifndef __ardour_marker_time_axis_view_h__
#define __ardour_marker_time_axis_view_h__

#include <list>
#include <gdkmm/color.h>

#include "ardour/location.h"

class PublicEditor;
class MarkerTimeAxis;
class ImageFrameView ;
class MarkerView ;
class TimeAxisView ;
class TimeAxisViewItem ;

/**
 * A view helper for handling MarkerView objects.
 * This object is responsible for the time axis canvas view, and
 * maintains the list of items that have been added to it
 */
class MarkerTimeAxisView : public sigc::trackable
{
	public:
		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * Construct a new MarkerTimeAxisView helper time axis helper
		 *
		 * @param mta the TimeAxsiView that this objbect is the helper for
		 */
		MarkerTimeAxisView(MarkerTimeAxis& mta) ;

		/**
		 * Destructor
		 * Reposinsibly for destroying all marker items that may have been added to this time axis view
		 *
		 */
		~MarkerTimeAxisView () ;

		//---------------------------------------------------------------------------------------//
		// Parent/Child helper object accessors

		/**
		 * Returns the TimeAxisView thatt his object is acting as a helper for
		 *
		 * @return the TimeAxisView that this object is acting as a view helper for
		 */
		MarkerTimeAxis& trackview() { return _trackview; }

		/**
		 *
		 */
		ArdourCanvas::Item *canvas_item() { return canvas_group; }


		//---------------------------------------------------------------------------------------//
		// ui methods & data

		/**
		 * Sets the height of the time axis view and the item upon it
		 *
		 * @param height the new height
		 */
		int set_height(gdouble height) ;

		/**
		 * Sets the position of this view helper on the canvas
		 *
		 * @param x the x position upon the canvas
		 * @param y the y position upon the canvas
		 */
		int set_position(gdouble x, gdouble y) ;

		int set_samples_per_pixel (double);

		/**
		 * Returns the current samples per unit of this time axis view helper
		 *
		 * @return the current samples per unit of this time axis view helper
		 */
		gdouble get_samples_per_pixel() { return _samples_per_pixel; }

		/**
		 * Sets the color of the items contained upon this view helper
		 *
		 * @param color the new base color
		 */
		void apply_color(Gdk::Color& color) ;

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
		MarkerView* add_marker_view(ImageFrameView* ifv, std::string mark_type, std::string mark_id, nframes_t start, nframes_t dur, void* src) ;

		/**
		 * Returns the named MarkerView or 0 if the named marker does not exist
		 *
		 * @param item_id the unique id of the item to search for
		 * @return the named MarkerView, or 0 if it is not held upon this view
		 */
		MarkerView* get_named_marker_view(std::string item_id) ;

		/**
		 * Removes the currently selected MarverView
		 * Note that this method actually destroys the MarkerView too.
		 * We assume that since we own the object, we are allowed to do this
		 *
		 * @param src the identity of the object that initiated the change
		 * @see add_marker_view
		 */
		void remove_selected_marker_view(void* src) ;

		/**
		 * Removes and returns the named MarkerView from the list of MarkerView held by this view helper
		 *
		 * @param item_id the MarkerView unique id to remove
		 * @param src the identity of the object that initiated the change
		 * @see add_marker_view
		 */
		MarkerView* remove_named_marker_view(std::string item_id, void* src) ;

		/**
		 * Removes mv from the list of MarkerView upon this TimeAxis
		 *
		 * @param mv the MarkerView to remove
		 * @param src the identity of the object that initiated the change
		 */
		void remove_marker_view(MarkerView* item, void* src) ;

		//---------------------------------------------------------------------------------------//
		// Selected item methods

		/**
		 * Sets the currently selected item upon this time axis
		 *
		 * @param mv the item to set selected
		 */
		void set_selected_time_axis_item(MarkerView* mv) ;

		/**
		 * Clears any selected item upon this time axis
		 *
		 */
		void clear_selected_time_axis_item() ;

		/**
		 * Returnsthe currently selected item upon this time axis
		 *
		 * @return the currently selected item pon this time axis
		 */
		MarkerView* get_selected_time_axis_item() ;


		/**
		 * Sets the duration of the selected MarkerView to the specified number of seconds
		 *
		 * @param sec the duration to set the MArkerView to, in seconds
		 */
		void set_marker_duration_sec(double sec) ;

		//---------------------------------------------------------------------------------//
		// Emitted Signals

		/** Emitted when a MarkerView is Added */
		sigc::signal<void,MarkerView*,void*> MarkerViewAdded ;

		/** Emitted when a MarkerView Item is removed */
		sigc::signal<void,std::string,void*> MarkerViewRemoved ;

	private:
		/**
		 * convenience method to re-get the samples per unit and tell items upon this view
		 *
		 */
		void reset_samples_per_pixel() ;

		/** The list of items held by this time axis view helper */
		typedef std::list<MarkerView *> MarkerViewList ;
		MarkerViewList marker_view_list;

		/** the currently selected time axis item upon this time axis */
		MarkerView* selected_time_axis_item ;

		/* the TimeAxisView that this object is acting as the view helper for */
		MarkerTimeAxis& _trackview ;

		ArdourCanvas::Group *canvas_group ;
		ArdourCanvas::Rectangle *canvas_rect; /* frame around the whole thing */

		/** the current frames per pixel */
		double _samples_per_pixel;

		/* XXX why are these different? */
		Gdk::Color region_color;
		uint32_t stream_base_color;

}; /* class MarkerTimeAxisView */

#endif /* __ardour_marker_time_axis_view_h__ */
