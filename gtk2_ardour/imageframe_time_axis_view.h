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

#ifndef __ardour_imageframe_time_axis_view_h__
#define __ardour_imageframe_time_axis_view_h__

#include <list>
#include <cmath>

#include <gdkmm/color.h>

#include <jack/jack.h>



class PublicEditor ;
class ImageFrameTimeAxis ;
class ImageFrameView ;
class ImageFrameTimeAxisGroup ;

/**
 * ImageFrameTimeAxisView defines the time axis view helper
 * This object is responsible for the time axis canvas view, and
 * maintains the list of items that have been added to it
 *
 */
class ImageFrameTimeAxisView : public sigc::trackable
{
	public:
		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * Constructs a new ImageFrameTimeAxisView.
		 *
		 * @param ifta the parent ImageFrameTimeAxis of this view helper
		 */
		ImageFrameTimeAxisView(ImageFrameTimeAxis& ifta) ;

		/**
		 * Destructor
		 * Responsible for destroying all items tat may have been added to this time axis
		 */
		~ImageFrameTimeAxisView () ;

		//---------------------------------------------------------------------------------------//
		// Parent/Child helper object accessors

		/**
		 * Returns the TimeAxisView thatt his object is acting as a helper for
		 *
		 * @return the TimeAxisView that this object is acting as a view helper for
		 */
		ImageFrameTimeAxis& trackview() { return _trackview; }

		/**
		 *
		 */
		ArdourCanvas::Group * canvas_item() { return &canvas_group; }


		//---------------------------------------------------------------------------------------//
		// ui methods & data

		/**
		 * Sets the height of the time axis view and the item upon it
		 *
		 * @param height the new height
		 */
		int set_height(gdouble) ;

		/**
		 * Sets the position of this view helper on the canvas
		 *
		 * @param x the x position upon the canvas
		 * @param y the y position upon the canvas
		 */
		int set_position(gdouble x, gdouble y) ;

		int set_samples_per_pixel (double);
		double get_samples_per_pixel () { return _samples_per_pixel; }

		/**
		 * Sets the color of the items contained uopn this view helper
		 *
		 * @param color the new base color
		 */
		void apply_color (Gdk::Color&) ;

		//---------------------------------------------------------------------------------------//
		// Child ImageFrameTimeAxisGroup Accessors/Mutators

		/**
		 * Adds an ImageFrameTimeAxisGroup to the list of items upon this time axis view helper
		 * the new ImageFrameTimeAxisGroup is returned
		 *
		 * @param group_id the unique id of the new group
		 * @param src the identity of the object that initiated the change
		 */
		ImageFrameTimeAxisGroup* add_imageframe_group(std::string group_id, void* src) ;

		/**
		 * Returns the named ImageFrameTimeAxisGroup or 0 if the named group does not exist on this view helper
		 *
		 * @param group_id the unique id of the group to search for
		 * @return the named ImageFrameTimeAxisGroup, or 0 if it is not held upon this view
		 */
		ImageFrameTimeAxisGroup* get_named_imageframe_group(std::string group_id) ;

		/**
		 * Removes and returns the named ImageFrameTimeAxisGroup from the list of ImageFrameTimeAxisGroup held by this view helper
		 *
		 * @param group_id the ImageFrameTimeAxisGroup unique id to remove
		 * @param src the identity of the object that initiated the change
		 * @see add_imageframe_group
		 */
		ImageFrameTimeAxisGroup* remove_named_imageframe_group(std::string group_id, void* src) ;

		/**
		 * Removes the specified ImageFrameTimeAxisGroup from the list of ImageFrameTimeAxisGroups upon this TimeAxis.
		 *
		 * @param iftag the ImageFrameView to remove
		 */
		void remove_imageframe_group(ImageFrameTimeAxisGroup* iftag, void* src) ;


		//---------------------------------------------------------------------------------------//
		// Selected group methods

		/**
		 * Sets the currently selected group upon this time axis
		 *
		 * @param ifv the item to set selected
		 */
		void set_selected_imageframe_group(ImageFrameTimeAxisGroup* iftag) ;

		/**
		 * Clears the currently selected image frame group unpo this time axis
		 *
		 */
		void clear_selected_imageframe_group() ;

		/**
		 * Returns the currently selected group upon this time axis
		 *
		 * @return the currently selected group upon this time axis
		 */
		ImageFrameTimeAxisGroup* get_selected_imageframe_group() const ;


		/**
		 * Sets the duration of the selected ImageFrameView to the specified number of seconds
		 *
		 * @param sec the duration to set the ImageFrameView to, in seconds
		 */
		void set_imageframe_duration_sec(double sec) ;

		//---------------------------------------------------------------------------------------//
		// Selected item methods

		/**
		 * Sets the currently selected image frame view item
		 *
		 * @param iftag the group the selected item is part
		 * @param ifv the selected item
		 */
		void set_selected_imageframe_view(ImageFrameTimeAxisGroup* iftag, ImageFrameView* ifv) ;

		/**
		 * Clears the currently selected image frame view item
		 *
		 * @param clear_group set true if the selected parent group of the item should be cleared also
		 */
		void clear_selected_imageframe_item(bool clear_group) ;

		/**
		 * Returns the currently selected image frame view item upon this time axis
		 *
		 * @return the currently selected image frame view item
		 */
		ImageFrameView* get_selected_imageframe_view() const ;



		/**
		 * Removes the currently selected ImageFrameTimeAxisGroup
		 *
		 * @param src the identity of the object that initiated the change
		 * @see add_imageframe_group
		 */
		void remove_selected_imageframe_item(void* src) ;


		//---------------------------------------------------------------------------------//
		// Emitted Signals

		/** Emitted when and ImageFrameGroup is added to this time axis */
		sigc::signal<void,ImageFrameTimeAxisGroup*,void*> ImageFrameGroupAdded ;

		/** Emitted when an ImageFrameGroup is removed from this time axis */
		sigc::signal<void,std::string,void*> ImageFrameGroupRemoved ;

	protected:


	private:
		/**
		 * convenience method to re-get the samples per unit and tell items upon this view
		 */
		void reset_samples_per_pixel ();

		/**
		 * The list of ImageFrameViews held by this view helper */
		typedef std::list<ImageFrameTimeAxisGroup *> ImageFrameGroupList ;
		ImageFrameGroupList imageframe_groups ;

		/** the currently selected time axis item upon this time axis */
		ImageFrameTimeAxisGroup* selected_imageframe_group ;

		/**
		 * thecurrently selected image frame view
		 * we keep this here so that we only have one per view, not one per group
		 */
		ImageFrameView* selected_imageframe_view ;



		/* the TimeAxisView that this object is acting as the view helper for */
		ImageFrameTimeAxis& _trackview ;

		ArdourCanvas::Group       canvas_group ;
		ArdourCanvas::Rectangle  canvas_rect; /* frame around the whole thing */

		/** the current frames per pixel */
		double _samples_per_pixel;

		/* XXX why are these different? */
		Gdk::Color region_color ;
		uint32_t stream_base_color ;

} ; /* class ImageFrameTimeAxisView */

#endif /* __ardour_imageframe_time_axis_view_h__ */
