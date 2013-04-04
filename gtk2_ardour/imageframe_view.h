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

#ifndef __gtk_ardour_imageframe_view_h__
#define __gtk_ardour_imageframe_view_h__

#include <string>
#include <gdkmm/color.h>
#include <sigc++/signal.h>
#include <list>

#include "enums.h"
#include "time_axis_view_item.h"
#include "marker_view.h"

class ImageFrameTimeAxis;
class ImageFrameTimeAxisGroup ;

/**
 * An ImageFrameItem to display an image upon the ardour time line
 *
 */
class ImageFrameView : public TimeAxisViewItem
{
	public:
		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * Constructs a new ImageFrameView upon the canvas
		 *
		 * @param item_id unique id of this item
		 * @param parent the parent canvas item
		 * @param tv the time axis view that this item is to be placed upon
		 * @param group the ImageFrameGroup that this item is a member of
		 * @param spu the current samples per canvas unit
		 * @param start the start frame ogf this item
		 * @param duration the duration of this item
		 * @param rgb_data the rgb data of the image
		 * @param width the width of the original rgb_data image data
		 * @param height the width of the origianl rgb_data image data
		 * @param num_channels the number of color channels within rgb_data
		 */
		ImageFrameView(const std::string & item_id,
			 ArdourCanvas::Group *parent,
			ImageFrameTimeAxis *tv,
			ImageFrameTimeAxisGroup* group,
			double spu,
		        Gdk::Color& base_color,
			framepos_t start,
			framecnt_t duration,
			unsigned char* rgb_data,
			uint32_t width,
			uint32_t height,
			uint32_t num_channels) ;

		/**
		 * Destructor
		 * Reposible for removing and destroying all marker items associated with this item
		 */
		~ImageFrameView() ;

		static PBD::Signal1<void,ImageFrameView*> CatchDeletion;

		//---------------------------------------------------------------------------------------//
		// Position and duration Accessors/Mutators

		/**
		 * Set the position of this item to the specified value
		 *
		 * @param pos the new position
		 * @param src the identity of the object that initiated the change
		 * @return true if the position change was a success, false otherwise
		 */
		virtual bool set_position(framepos_t pos, void* src, double* delta = 0) ;

		/**
		 * Sets the duration of this item
		 *
		 * @param dur the new duration of this item
		 * @param src the identity of the object that initiated the change
		 * @return true if the duration change was succesful, false otherwise
		 */
		virtual bool set_duration(framepos_t dur, void* src) ;

		//---------------------------------------------------------------------------------------//
		// Parent Component Methods

		/**
		 * Sets the parent ImageFrameTimeAxisGroup of thie item
		 * each Item must be part of exactly one group (or 'scene') upon the timeline
		 *
		 * @param group the new parent group
		 */
		void set_time_axis_group(ImageFrameTimeAxisGroup* group) ;

		/**
		 * Returns the parent group of this item
		 *
		 * @return the parent group of this item
		 */
		ImageFrameTimeAxisGroup* get_time_axis_group() ;

		//---------------------------------------------------------------------------------------//
		// ui methods

		/**
		 * Set the height of this item
		 *
		 * @param h the new height
		 */
		virtual void set_height(gdouble h) ;


		//---------------------------------------------------------------------------------------//
		// MarkerView methods

		/**
		 * Adds a markerView to the list of marker views associated with this item
		 *
		 * @param item the marker item to add
		 * @param src the identity of the object that initiated the change
		 */
		void add_marker_view_item(MarkerView* item, void* src) ;

		/**
		 * Removes the named marker view from the list of marker view associated with this item
		 * The Marker view is not destroyed on removal, so the caller must handle the item themself
		 *
		 * @param markId the id/name of the item to remove
		 * @param src the identity of the object that initiated the change
		 * @return the removed marker item
		 */
		MarkerView* remove_named_marker_view_item(const std::string & markId, void* src) ;

		/**
		 * Removes item from the list of marker views assocaited with this item
		 * This method will do nothing if item if not assiciated with this item
		 * The Marker view is not destroyed on removal, so the caller must handle the item themself
		 *
		 * @param item the item to remove
		 * @param src the identity of the object that initiated the change
		 */
		void remove_marker_view_item(MarkerView* item, void* src) ;

		/**
		 * Determines if the named marker is one of those associated with this item
		 *
		 * @param markId the id/name of the item to search for
		 */
		bool has_marker_view_item(const std::string & markId) ;


		//---------------------------------------------------------------------------------//
		// Emitted Signals

		/** Emitted when a marker Item is added to this Item */
		sigc::signal<void,MarkerView*,void*> MarkerViewAdded ;

		/** Emitted when a Marker Item is added to this Item */
		sigc::signal<void,MarkerView*,void*> MarkerViewRemoved ;

	private:
		/** the list of MarkerViews associated with this item */
		typedef std::list<MarkerView*> MarkerViewList ;
		MarkerViewList marker_view_list ;


		/** The parent group that this item is a member of */
		ImageFrameTimeAxisGroup* the_parent_group ;

		// ------- Image data -----------

		/** the image data that we display */
		//unsigned char* the_rgb_data ;

		/** The width of the image contained within the_rgb_data */
		uint32_t image_data_width ;

		/** The height of the image contained within the_rgb_data */
		uint32_t image_data_height ;

		/** the number of channels contained in the_rgb_data */
		uint32_t image_data_num_channels ;


		// ------- Our canvas element -----------

		/** the CanvasImageFrame to display the image */
		ArdourCanvas::ImageFrame* imageframe ;

} ; /* class ImageFrameView */

#endif /* __gtk_ardour_imageframe_view_h__ */
