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

#include "imageframe_time_axis_group.h"
#include "imageframe_time_axis_view.h"
#include "imageframe_view.h"
#include "imageframe_time_axis.h"
#include "region_selection.h"
#include "public_editor.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;

PBD::Signal1<void,ImageFrameTimeAxisGroup*> ImageFrameTimeAxisGroup::CatchDeletion;

//---------------------------------------------------------------------------------------//
// Constructor / Desctructor

/**
 * Constructs a new ImageFrameTimeAxisGroup.
 *
 * @param iftav the parent ImageFrameTimeAxis of this view helper
 * @param group_id the unique name/id of this group
 */
ImageFrameTimeAxisGroup::ImageFrameTimeAxisGroup(ImageFrameTimeAxisView& iftav, const string & group_id)
	: _view_helper(iftav), _group_id(group_id)
{
	selected_imageframe_item = 0;
	is_selected = false;

	ImageFrameView::CatchDeletion.connect (*this, boost::bind (&ImageFrameTimeAxisGroup::remove_imageframe_item, this, _1), gui_context());
}

/**
 * Destructor
 * Responsible for destroying any Items that may have been added to this group
 *
 */
ImageFrameTimeAxisGroup::~ImageFrameTimeAxisGroup()
{
	// Destroy all the ImageFramViews that we have
	for(ImageFrameViewList::iterator iter = imageframe_views.begin(); iter != imageframe_views.end(); ++iter)
	{
		ImageFrameView* ifv = *iter;

		ImageFrameViewList::iterator next = iter;
		next++;

		imageframe_views.erase(iter);

		delete ifv;
		ifv = 0;

		iter = next;
	}

	 CatchDeletion; /* EMIT_SIGNAL */
}


//---------------------------------------------------------------------------------------//
// Name/Id Accessors/Mutators

/**
 * Set the name/Id of this group.
 *
 * @param new_name the new name of this group
 * @param src the identity of the object that initiated the change
 */
void
ImageFrameTimeAxisGroup::set_group_name(const string & new_name, void* src)
{
	if(_group_id != new_name)
	{
		std::string temp_name = _group_id;
		_group_id = new_name;
		 NameChanged(_group_id, temp_name, src); /* EMIT_SIGNAL */
	}
}

/**
 * Returns the id of this group
 * The group id must be unique upon a time axis
 *
 * @return the id of this group
 */
std::string
ImageFrameTimeAxisGroup::get_group_name() const
{
	return(_group_id);
}


//---------------------------------------------------------------------------------------//
// ui methods & data

/**
 * Sets the height of the time axis view and the item upon it
 *
 * @param height the new height
 */
int
ImageFrameTimeAxisGroup::set_item_heights(gdouble h)
{
	/* limit the values to something sane-ish */
	if (h < 10.0 || h > 1000.0)
	{
		return(-1);
	}

	// set the heights of all the imaeg frame views within the group
	for(ImageFrameViewList::const_iterator citer = imageframe_views.begin(); citer != imageframe_views.end(); ++citer)
	{
		(*citer)->set_height(h);
	}

	return(0);
}

/**
 * Sets the current samples per unit.
 * this method tells each item upon the time axis of the change
 *
 * @param spu the new samples per canvas unit value
 */
int
ImageFrameTimeAxisGroup::set_item_frames_per_pixel (double fpp)
{
	if (fpp < 1.0) {
		return -1;
	}

	for (ImageFrameViewList::const_iterator citer = imageframe_views.begin(); citer != imageframe_views.end(); ++citer) {
		(*citer)->set_frames_per_pixel (fpp);
	}

	return 0;
}

/**
 * Sets the color of the items contained uopn this view helper
 *
 * @param color the new base color
 */
void
ImageFrameTimeAxisGroup::apply_item_color(Gdk::Color& color)
{
	region_color = color;
	for(ImageFrameViewList::const_iterator citer = imageframe_views.begin(); citer != imageframe_views.end(); citer++)
	{
		(*citer)->set_color (region_color);
	}
}



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
ImageFrameView*
ImageFrameTimeAxisGroup::add_imageframe_item(const string & frame_id, framepos_t start, framecnt_t duration, unsigned char* rgb_data, uint32_t width, uint32_t height, uint32_t num_channels, void* src)
{
	ImageFrameView* ifv = 0;

	//check that there is not already an imageframe with that id
	if(get_named_imageframe_item(frame_id) == 0)
	{
		ifv = new ImageFrameView(frame_id,
			_view_helper.canvas_item()->property_parent(),
			&(_view_helper.trackview()),
			this,
			_view_helper.trackview().editor.get_current_zoom(),
			region_color,
			start,
			duration,
			rgb_data,
			width,
			height,
			num_channels);

		imageframe_views.push_front(ifv);
		ImageFrameAdded(ifv, src); /* EMIT_SIGNAL */
	}

	return(ifv);
}


/**
 * Returns the named ImageFrameView or 0 if the named view does not exist on this view helper
 *
 * @param item_id the unique id of the item to search for
 * @return the named ImageFrameView, or 0 if it is not held upon this view
 */
ImageFrameView*
ImageFrameTimeAxisGroup::get_named_imageframe_item(const string & frame_id)
{
	ImageFrameView* ifv =  0;

	for (ImageFrameViewList::const_iterator i = imageframe_views.begin(); i != imageframe_views.end(); ++i)
	{
		if (((ImageFrameView*)*i)->get_item_name() == frame_id)
		{
			ifv = ((ImageFrameView*)*i);
			break;
		}
	}
	return(ifv);
}

/**
 * Removes the currently selected ImageFrameView
 *
 * @param src the identity of the object that initiated the change
 * @todo need to remoev this, the selected item within group is no longer
 *       used in favour of a time axis selected item
 * @see add_imageframe_view
 */
void
ImageFrameTimeAxisGroup::remove_selected_imageframe_item(void* src)
{
	std::string frame_id;

	if(selected_imageframe_item)
	{
		ImageFrameViewList::iterator i;

		if((i = find(imageframe_views.begin(), imageframe_views.end(), selected_imageframe_item)) != imageframe_views.end())
		{
			imageframe_views.erase(i);
			frame_id = selected_imageframe_item->get_item_name();

			// note that we delete the item here
			delete(selected_imageframe_item);
			selected_imageframe_item = 0;

			std::string track_id = _view_helper.trackview().name();
			 ImageFrameRemoved(track_id, _group_id, frame_id, src); /* EMIT_SIGNAL */
		}
	}
	else
	{
		//cerr << "No Selected ImageFrame" << endl;
	}
}


/**
 * Removes and returns the named ImageFrameView from the list of ImageFrameViews held by this view helper
 *
 * @param item_id the ImageFrameView unique id to remove
 * @param src the identity of the object that initiated the change
 * @see add_imageframe_view
 */
ImageFrameView*
ImageFrameTimeAxisGroup::remove_named_imageframe_item(const string & frame_id, void* src)
{
	ImageFrameView* removed = 0;

	for(ImageFrameViewList::iterator iter = imageframe_views.begin(); iter != imageframe_views.end(); ++iter)
	{
		ImageFrameView* tempItem = *iter;
		if(tempItem->get_item_name() == frame_id)
		{
			removed = tempItem;
			imageframe_views.erase(iter);

			if (removed == selected_imageframe_item)
			{
				selected_imageframe_item = 0;
			}

			std::string track_id = _view_helper.trackview().name();
			 ImageFrameRemoved(track_id, _group_id, frame_id, src); /* EMIT_SIGNAL */

			// break from the for loop
			break;
		}
		iter++;
	}

	return(removed);
}

/**
 * Removes ifv from the list of ImageFrameViews upon this TimeAxis.
 * if ifv is not upon this TimeAxis, this method takes no action
 *
 * @param ifv the ImageFrameView to remove
 */
void
ImageFrameTimeAxisGroup::remove_imageframe_item (ImageFrameView* ifv)
{
	ENSURE_GUI_THREAD (*this, &ImageFrameTimeAxisGroup::remove_imageframe_item, ifv, src)

	ImageFrameViewList::iterator i;

	if((i = find (imageframe_views.begin(), imageframe_views.end(), ifv)) != imageframe_views.end()) {
		imageframe_views.erase(i);

		std::string frame_id = ifv->get_item_name();
		std::string track_id = _view_helper.trackview().name();
		 ImageFrameRemoved(track_id, _group_id, frame_id, src); /* EMIT_SIGNAL */
	}
}

//---------------------------------------------------------------------------------------//
// Selected group methods

/**
 * Sets the currently selected item upon this time axis
 *
 * @param ifv the item to set selected
 */
//void
//ImageFrameTimeAxisGroup::set_selected_imageframe_item(ImageFrameView* ifv)
//{
//	if(selected_imageframe_item)
//	{
//		selected_imageframe_item->set_selected(false, this);
//	}
//
//	selected_imageframe_item = ifv;
//
//	if(!ifv->get_selected())
//	{
//		selected_imageframe_item->set_selected(true, this);
//	}
//}


/**
 * Sets the currently selected item upon this time axis to the named item
 *
 * @param item_id the name/id of the item to set selected
 */
//void
//ImageFrameTimeAxisGroup::set_selected_imageframe_item(std::string frame_id)
//{
//	selected_imageframe_item = get_named_imageframe_item(frame_id);
//}


/**
 * Returns the currently selected item upon this time axis
 *
 * @return the currently selected item pon this time axis
 */
// ImageFrameView*
// ImageFrameTimeAxisGroup::get_selected_imageframe_item()
// {
	// return(selected_imageframe_item);
// }



/**
 * Returns whether this grou pis currently selected
 *
 * @returns true if this group is currently selected
 */
bool
ImageFrameTimeAxisGroup::get_selected() const
{
	return(is_selected);
}


/**
 * Sets he selected state of this group
 *
 * @param yn set true if this group is selected, false otherwise
 */
void
ImageFrameTimeAxisGroup::set_selected(bool yn)
{
	is_selected = yn;
}



//---------------------------------------------------------------------------------------//
// Handle time axis removal

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
void
ImageFrameTimeAxisGroup::remove_this_group(void* src)
{
	/*
	   defer to idle loop, otherwise we'll delete this object
	   while we're still inside this function ...
	*/
  	Glib::signal_idle().connect(sigc::bind(ptr_fun(&ImageFrameTimeAxisGroup::idle_remove_this_group), this, src));
}

/**
 * Callback used to remove this group during the gtk idle loop
 * This is used to avoid deleting the obejct while inside the remove_this_group
 * method
 *
 * @param group the ImageFrameTimeAxisGroup to remove
 * @param src the identity of the object that initiated the change
 */
gint
ImageFrameTimeAxisGroup::idle_remove_this_group(ImageFrameTimeAxisGroup* group, void* src)
{
	delete group;
	group = 0;
	 group->GroupRemoved(group->get_group_name(), src); /* EMIT_SIGNAL */
	return(false);
}

