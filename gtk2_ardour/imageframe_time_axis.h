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

#ifndef __ardour_imageframe_time_axis_h__
#define __ardour_imageframe_time_axis_h__

#include <list>


#include "ardour_dialog.h"
#include "enums.h"
#include "time_axis_view.h"
#include "visual_time_axis.h"

namespace ARDOUR
{
	class Session ;
}
namespace Gtk {
	class Menu;
}

class PublicEditor ;
class ImageFrameView ;
class ImageFrameTimeAxisView ;
class MarkersTimeAxisView ;
class MarkerTimeAxis;

/**
 * ImageFrameTimeAxis defines a visual time axis view for holding and arranging image items.
 *
 */
class ImageFrameTimeAxis : public VisualTimeAxis
{
	public:
		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * Constructs a new ImageFrameTimeAxis.
		 *
		 * @param track_id the track name/id
		 * @param ed the PublicEditor
		 * @param sess the current session
		 * @param canvas the parent canvas item
		 */
		ImageFrameTimeAxis(const std::string & track_id, PublicEditor& ed, ARDOUR::Session* sess, ArdourCanvas::Canvas& canvas) ;

		/**
		 * Destructor
		 * Responsible for destroying any child image items that may have been added to thie time axis
		 */
		virtual ~ImageFrameTimeAxis() ;

		//---------------------------------------------------------------------------------------//
		// ui methods & data

		/**
		 * Sets the height of this TrackView to one of ths TrackHeghts
		 *
		 * @param h the number of pixels to set the height too
		 */
		virtual void set_height(uint32_t h) ;

		virtual void set_frames_per_pixel (double);

		/**
		 * Returns the available height for images to be drawn onto
		 *
		 * @return the available height for an image item to be drawn onto
		 */
		int get_image_display_height() ;


		/**
		 * Show the popup edit menu
		 *
		 * @param button the mouse button pressed
		 * @param time when to show the popup
		 * @param clicked_imageframe the ImageFrameItem that the event ocured upon, or 0 if none
		 * @param with_item true if an item has been selected upon the time axis, used to set context menu
		 */
		void popup_imageframe_edit_menu(int button, int32_t time, ImageFrameView* clicked_imageframe, bool with_item) ;


		//---------------------------------------------------------------------------------------//
		// Marker Time Axis Methods

		/**
		 * Add a MarkerTimeAxis to the ilst of MarkerTimeAxis' associated with this ImageFrameTimeAxis
		 *
		 * @param marker_track the MarkerTimeAxis to add
		 * @param src the identity of the object that initiated the change
		 * @return true if the addition was a success,
		 *         false otherwise
		 */
		bool add_marker_time_axis(MarkerTimeAxis* marker_track, void* src) ;

		/**
		 * Returns the named MarkerTimeAxis associated with this ImageFrameTimeAxis
		 *
		 * @param track_id the track_id of the MarkerTimeAxis to search for
		 * @return the named markerTimeAxis, or 0 if the named MarkerTimeAxis is not associated with this ImageFrameTimeAxis
		 */
		MarkerTimeAxis* get_named_marker_time_axis(const std::string & track_id) ;

		/**
		 * Removes the named markerTimeAxis from those associated with this ImageFrameTimeAxis
		 *
		 * @param track_id the track id of the MarkerTimeAxis to remove
		 * @param src the identity of the object that initiated the change
		 * @return the removed MarkerTimeAxis
		 */
		MarkerTimeAxis* remove_named_marker_time_axis(const std::string & track_id, void* src) ;

		/**
		 * Potentially removes a MarkerTimeAxisView from the list of MarkerTimaAxis associated with this ImageFrameTimeAxis
		 *
		 * @param tav the TimeAxis to remove
		 * @param src the identity of the object that initiated the change
		 */
		void remove_time_axis_view (TimeAxisView* av);


		//---------------------------------------------------------------------------------------//
		// Parent/Child helper object accessors

		/**
		 * Returns the view helper of this TimeAxis
		 *
		 * @return the view helper of this TimeAxis
		 */
		ImageFrameTimeAxisView* get_view() ;


		//---------------------------------------------------------------------------------//
		// Emitted Signals

		/** Emitted when a Marker Time Axis is Added, or associated with, this time axis */
		sigc::signal<void,MarkerTimeAxis*,void*> MarkerTimeAxisAdded ;

		/** Emitted when a Marker Time Axis is removed, from this time axis */
		sigc::signal<void,std::string,void*> MarkerTimeAxisRemoved ;

	protected:

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
		 * handles the building of the ImageFrameView sub menu
		 */
		void create_imageframe_menu() ;

  		/* We may have multiple marker views, but each marker view should only be associated with one timeaxisview */
		typedef std::list<MarkerTimeAxis*> MarkerTimeAxisList ;
		MarkerTimeAxisList marker_time_axis_list;

		/* the TimeAxis view helper */
		ImageFrameTimeAxisView *view ;

		// popup menu widgets
		Gtk::Menu *image_action_menu ;
		Gtk::Menu *imageframe_menu ;
		Gtk::Menu *imageframe_item_menu ;

}; /* class ImageFrameTimeAxis */

#endif /* __ardour_imageframe_time_axis_h__ */

