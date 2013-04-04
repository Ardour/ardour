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

#ifndef __ardour_marker_time_axis_h__
#define __ardour_marker_time_axis_h__

#include <string>

#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "time_axis_view.h"
#include "visual_time_axis.h"

namespace ARDOUR {
	class Session;
}

class PublicEditor;
class ImageFrameView ;
class ImageFrameTimeAxisView ;
class MarkerTimeAxisView ;
class MarkerView ;

/**
 * MarkerTimeAxis defines a visual time axis for holding marker items associated with other time axis, and time axis items.
 *
 * The intention of this time axis is to allow markers with duration to be arranged on the time line
 * to add additional timing information to items on an associated time axis, for instance the addition
 * of effect duration and timings
 */
class MarkerTimeAxis : public VisualTimeAxis
{
	public:
		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * Constructs a new MarkerTimeAxis
		 *
		 * @param ed the PublicEditor
		 * @param sess the current session
		 * @param canvas the parent canvas item
		 * @param name the name/id of this time axis
		 * @param tav the associated track view that this MarkerTimeAxis is marking up
		 */
		MarkerTimeAxis(PublicEditor& ed, ARDOUR::Session* sess, ArdourCanvas::Canvas& canvas, const std::string & name, TimeAxisView* tav) ;

		/**
		 * Destructor
		 * Responsible for destroying any marker items upon this time axis
		 */
		virtual ~MarkerTimeAxis() ;


		//---------------------------------------------------------------------------------------//
		// ui methods & data

		/**
		 * Sets the height of this TrackView to one of the defined TrackHeights
		 *
		 * @param h the number of pixels to set the height to
		 */
		virtual void set_height(uint32_t h) ;

		virtual void set_frames_per_pixel (double);


		/**
		 * Show the popup edit menu
		 *
		 * @param button the mouse button pressed
		 * @param time when to show the popup
		 * @param clicked_mv the MarkerView that the event ocured upon, or 0 if none
		 * @param with_item true if an item has been selected upon the time axis, used to set context menu
		 */
		void popup_marker_time_axis_edit_menu(int button, int32_t time, MarkerView* clicked_mv, bool with_item) ;


		//---------------------------------------------------------------------------------------//
		// Parent/Child helper object accessors

		/**
		 * Returns the view helper of this TimeAxis
		 *
		 * @return the view helper of this TimeAxis
		 */
		MarkerTimeAxisView* get_view() ;

		/**
		 * Returns the TimeAxisView that this markerTimeAxis is marking up
		 *
		 * @return the TimeAXisView that this MarkerTimeAxis is marking
		 */
		TimeAxisView* get_marked_time_axis() ;


	private:

		/**
		 * convenience method to select a new track color and apply it to the view and view items
		 *
		 */
		void select_track_color() ;

		/**
		 * Handles the building of the popup menu
		 */
		virtual void build_display_menu() ;

		/**
		 * handles the building of the MarkerView sub menu
		 */
		void build_marker_menu() ;

		/** The associated TimeAxis that this MarkerTimeAxis is marking up */
		TimeAxisView* marked_time_axis ;

		/** Our time axis view helper */
		MarkerTimeAxisView *view ;

		/** the popup menu available by clicking upon this time axis */
		Gtk::Menu *marker_menu ;

		/** specialized sub menu available when clicking upon and item upon this time axis */
		Gtk::Menu *marker_item_menu ;


} ; /* class MarkerTimeAxis */

#endif /* __ardour_imageframe_time_axis_h__ */

