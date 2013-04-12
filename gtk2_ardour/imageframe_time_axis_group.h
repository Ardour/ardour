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

#ifndef __ardour_imageframe_time_axis_group_h__
#define __ardour_imageframe_time_axis_group_h__

#include <list>
#include <cmath>

#include <gdkmm/color.h>

#include <jack/jack.h>
#include "ardour/types.h"
#include "imageframe_time_axis_view.h"

class PublicEditor ;
class ImageFrameView ;

/**
 * ImageFrameTimeAxisGroup defines a group/scene of ImageFrame view that can appear upon a time axis
 * At the moment this is a bit bare, we really want to add some kind of time constraints upon
 * items atht are added to the group, ie bounded by the start and end of the scene, which itself
 * needs fleshed out.
 * A viewable object may also be useful...
 *
 */
class ImageFrameTimeAxisGroup : public sigc::trackable
{
	public:
		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * Constructs a new ImageFrameTimeAxisGroup.
		 *
		 * @param iftav the parent ImageFrameTimeAxis of this view helper
		 * @param group_id the unique name/id of this group
		 */
		ImageFrameTimeAxisGroup(ImageFrameTimeAxisView& iftav, const std::string & group_id) ;

		/**
		 * Destructor
		 * Responsible for destroying any Items that may have been added to this group
		 *
		 */
		virtual ~ImageFrameTimeAxisGroup() ;


		//---------------------------------------------------------------------------------------//
		// Name/Id Accessors/Mutators

		/**
		 * Set the name/Id of this group.
		 *
		 * @param new_name the new name of this group
		 * @param src the identity of the object that initiated the change
		 */
		void set_group_name(const std::string & new_name, void* src) ;

		/**
		 * Returns the id of this group
		 * The group id must be unique upon a time axis
		 *
		 * @return the id of this group
		 */
		std::string get_group_name() const ;


		//---------------------------------------------------------------------------------------//
		// Parent/Child helper object accessors

		/**
		 * Returns the TimeAxisView thatt his object is acting as a helper for
		 *
		 * @return the TimeAxisView that this object is acting as a view helper for
		 */
		ImageFrameTimeAxisView& get_view() const { return _view_helper ; }



		//---------------------------------------------------------------------------------------//
		// ui methods & data

		/**
		 * Sets the height of the time axis view and the item upon it
		 *
		 * @param height the new height
		 */
		int set_item_heights(gdouble) ;

		int set_item_samples_per_pixel (double);

		/**
		 * Sets the color of the items contained uopn this view helper
		 *
		 * @param color the new base color
		 */
		void apply_item_color(Gdk::Color&) ;


		//---------------------------------------------------------------------------------------//
		// child ImageFrameView methods

		/**
		 * Adds an ImageFrameView to the list of items upon this time axis view helper
		 * the new ImageFrameView is returned
		 *
		 * @param item_id the unique id of the new item
		 * @param image_id the id/name of the image data we are usin
		 * @param start the position the new item should be placed upon the time line
		 * @param duration the duration the new item should be placed upon the timeline
		 * @param rgb_data the rgb data of the image
		 * @param width the original image width of the rgb_data (not the size to display)
		 * @param height the irigianl height of the rgb_data
		 * @param num_channels the number of channles within the rgb_data
		 * @param src the identity of the object that initiated the change
		 */
		ImageFrameView* add_imageframe_item(const std::string & item_id, nframes_t start, nframes_t duration, unsigned char* rgb_data, uint32_t width, uint32_t height, uint32_t num_channels, void* src) ;

		/**
		 * Returns the named ImageFrameView or 0 if the named view does not exist on this view helper
		 *
		 * @param item_id the unique id of the item to search for
		 * @return the named ImageFrameView, or 0 if it is not held upon this view
		 */
		ImageFrameView* get_named_imageframe_item(const std::string & item_id) ;

		/**
		 * Removes the currently selected ImageFrameView
		 *
		 * @param src the identity of the object that initiated the change
		 * @see add_imageframe_view
		 */
		void remove_selected_imageframe_item(void* src) ;

		/**
		 * Removes and returns the named ImageFrameView from the list of ImageFrameViews held by this view helper
		 *
		 * @param item_id the ImageFrameView unique id to remove
		 * @param src the identity of the object that initiated the change
		 * @see add_imageframe_view
		 */
		ImageFrameView* remove_named_imageframe_item(const std::string & item_id, void* src) ;

		/**
		 * Removes ifv from the list of ImageFrameViews upon this TimeAxis.
		 * if ifv is not upon this TimeAxis, this method takes no action
		 *
		 * @param ifv the ImageFrameView to remove
		 */
		void remove_imageframe_item(ImageFrameView*, void* src) ;


		//---------------------------------------------------------------------------------------//
		// Selected group methods


		// removed in favour of a track level selectewd item
		// this is simply easier to manage a singularly selected item, rather than
		// a selected item within each group

		/**
		 * Sets the currently selected item upon this time axis
		 *
		 * @param ifv the item to set selected
		 */
		//void set_selected_imageframe_item(ImageFrameView* ifv) ;

		/**
		 * Sets the currently selected item upon this time axis to the named item
		 *
		 * @param item_id the name/id of the item to set selected
		 */
		//void set_selected_imageframe_item(std::string item_id) ;

		/**
		 * Returns the currently selected item upon this time axis
		 *
		 * @return the currently selected item pon this time axis
		 */
		//ImageFrameView* get_selected_imageframe_item() ;

		/**
		 * Returns whether this grou pis currently selected
		 *
		 * @returns true if this group is currently selected
		 */
		bool get_selected() const ;

		/**
		 * Sets he selected state of this group
		 *
		 * @param yn set true if this group is selected, false otherwise
		 */
		void set_selected(bool yn) ;

		//---------------------------------------------------------------------------------------//
		// Handle group removal

		/**
		 * Handles the Removal of this VisualTimeAxis
		 * This _needs_ to be called to alert others of the removal properly, ie where the source
		 * of the removal came from.
		 *
		 * XXX Although im not too happy about this method of doing things, I cant think of a cleaner method
		 *     just now to capture the source of the removal
		 *
		 * @param src the identity of the object that initiated the change
		 */
		virtual void remove_this_group(void* src) ;

		//---------------------------------------------------------------------------------//
		// Emitted Signals

		static sigc::signal<void,ImageFrameTimeAxisGroup*> CatchDeletion;

		/**
		 * Emitted when this Group has been removed
		 * This is different to the CatchDeletion signal in that this signal
		 * is emitted during the deletion of this Time Axis, and not during
		 * the destructor, this allows us to capture the source of the deletion
		 * event
		 */
		sigc::signal<void,std::string,void*> GroupRemoved ;

		/** Emitted when we have changed the name of this TimeAxis */
		sigc::signal<void,std::string,std::string,void*> NameChanged ;

		/** Emitted when an ImageFrameView is added to this group */
		sigc::signal<void, ImageFrameView*, void*> ImageFrameAdded ;

		/** Emitted when an ImageFrameView is removed from this group */
		sigc::signal<void, const std::string &, const std::string &, const std::string &, void*> ImageFrameRemoved ;

	protected:


	private:
		/**
		 * convenience method to re-get the samples per unit and tell items upon this view
		 *
		 */
		void reset_samples_per_pixel ();

		/**
		 * Callback used to remove this group during the gtk idle loop
		 * This is used to avoid deleting the obejct while inside the remove_this_group
		 * method
		 *
		 * @param group the ImageFrameTimeAxisGroup to remove
		 * @param src the identity of the object that initiated the change
		 */
		static gint idle_remove_this_group(ImageFrameTimeAxisGroup* group, void* src) ;

		/** The list of ImageFrameViews held by this view helper */
		typedef std::list<ImageFrameView *> ImageFrameViewList ;
		ImageFrameViewList imageframe_views ;

		/** the currently selected time axis item upon this time axis */
		ImageFrameView* selected_imageframe_item ;

		/** the view helper that this object is acting as a container upon on */
		ImageFrameTimeAxisView& _view_helper ;

		/** the is of this group */
		std::string _group_id ;

		/* XXX why are these different? */
		Gdk::Color region_color ;
		uint32_t stream_base_color ;

		/** indicates if this group is currently selected */
		bool is_selected ;

} ; /* class ImageFrameTimeAxisGroup */

#endif /* __ardour_imageframe_time_axis_group_h__ */
