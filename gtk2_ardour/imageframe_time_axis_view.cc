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

#include "imageframe_time_axis_view.h"
#include "imageframe_time_axis_group.h"
#include "imageframe_view.h"
#include "imageframe_time_axis.h"
#include "region_selection.h"
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
 * Constructs a new ImageFrameTimeAxisView.
 *
 * @param ifta the parent ImageFrameTimeAxis of this view helper
 */
ImageFrameTimeAxisView::ImageFrameTimeAxisView (ImageFrameTimeAxis& tv)
	: _trackview (tv),
	  canvas_group (*_trackview.canvas_display),
	  canvas_rect (canvas_group, 0.0, 0.0, 1000000.0, tv.current_height())
{
	region_color = _trackview.color() ;
	stream_base_color = ARDOUR_UI::config()->canvasvar_ImageTrack.get() ;

	canvas_rect.property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_ImageTrack.get();
	canvas_rect.property_fill_color_rgba() = stream_base_color;

	canvas_rect.signal_event().connect (sigc::bind (sigc::mem_fun (_trackview.editor, &PublicEditor::canvas_imageframe_view_event), (ArdourCanvas::Item*) &canvas_rect, &tv));

	_samples_per_pixel = _trackview.editor.get_current_zoom() ;

	_trackview.editor.ZoomChanged.connect (sigc::mem_fun(*this, &ImageFrameTimeAxisView::reset_samples_per_pixel)) ;

	selected_imageframe_group = 0 ;
	selected_imageframe_view = 0 ;

	ImageFrameTimeAxisGroup::CatchDeletion.connect (*this, boost::bind (&ImageFrameTimeAxisView::remove_imageframe_group, this, _1), gui_context());
}

/**
 * Destructor
 * Responsible for destroying all items tat may have been added to this time axis
 */
ImageFrameTimeAxisView::~ImageFrameTimeAxisView()
{
	// Destroy all the ImageFrameGroups that we have

	for(ImageFrameGroupList::iterator iter = imageframe_groups.begin(); iter != imageframe_groups.end(); ++iter)
	{
		ImageFrameTimeAxisGroup* iftag = (*iter) ;

		ImageFrameGroupList::iterator next = iter ;
		next++ ;

		// remove the front element
		imageframe_groups.erase(iter) ;

		delete iftag ;
		iftag = 0 ;

		iter = next ;
	}

}


//---------------------------------------------------------------------------------------//
// ui methods & data

/**
 * Sets the height of the time axis view and the item upon it
 *
 * @param height the new height
 */
int
ImageFrameTimeAxisView::set_height (gdouble h)
{
	/* limit the values to something sane-ish */
	if (h < 10.0 || h > 1000.0) {
		return(-1) ;
	}

	canvas_rect.property_y2() = h ;


	for(ImageFrameGroupList::const_iterator citer = imageframe_groups.begin(); citer != imageframe_groups.end(); ++citer)
	{
		(*citer)->set_item_heights(h) ;
	}

	return(0) ;
}

/**
 * Sets the position of this view helper on the canvas
 *
 * @param x the x position upon the canvas
 * @param y the y position npon the canvas
 */
int
ImageFrameTimeAxisView::set_position (gdouble x, gdouble y)

{
	canvas_group.property_x() = x;
	canvas_group.property_y() = y;

	return 0;
}

/**
 * Sets the current samples per unit.
 * this method tells each item upon the time axis of the change
 *
 * @param spu the new samples per canvas unit value
 */
int
ImageFrameTimeAxisView::set_samples_per_pixel (double fpp)
{
	if (fpp < 1.0) {
		return -1;
	}

	_samples_per_pixel = fpp;

	for (ImageFrameGroupList::const_iterator citer = imageframe_groups.begin(); citer != imageframe_groups.end(); ++citer) {
		(*citer)->set_item_samples_per_pixels (fpp);
	}

	return 0;
}

/**
 * Sets the color of the items contained uopn this view helper
 *
 * @param color the new base color
 */
void
ImageFrameTimeAxisView::apply_color(Gdk::Color& color)
{
	region_color = color ;
	for(ImageFrameGroupList::const_iterator citer = imageframe_groups.begin(); citer != imageframe_groups.end(); citer++)
	{
		(*citer)->apply_item_color(region_color) ;
	}
}


/**
 * convenience method to re-get the samples per unit and tell items upon this view
 *
 */
void
ImageFrameTimeAxisView::reset_samples_per_pixel ()
{
	set_samples_per_pixel (_trackview.editor.get_current_zoom());
}


//---------------------------------------------------------------------------------------//
// Child ImageFrameTimeAxisGroup Accessors/Mutators

/**
 * Adds an ImageFrameTimeAxisGroup to the list of items upon this time axis view helper
 * the new ImageFrameTimeAxisGroup is returned
 *
 * @param group_id the unique id of the new group
 * @param src the identity of the object that initiated the change
 */
ImageFrameTimeAxisGroup*
ImageFrameTimeAxisView::add_imageframe_group(std::string group_id, void* src)
{
	ImageFrameTimeAxisGroup* iftag = 0 ;

	//check that there is not already a group with that id
	if(get_named_imageframe_group(group_id) != 0)
	{
		// iftag = 0 ;
	}
	else
	{
		iftag = new ImageFrameTimeAxisGroup(*this, group_id) ;
		imageframe_groups.push_front(iftag) ;
		ImageFrameGroupAdded(iftag, src) ; /* EMIT_SIGNAL */
	}

	return(iftag) ;
}

/**
 * Returns the named ImageFrameTimeAxisGroup or 0 if the named group does not exist on this view helper
 *
 * @param group_id the unique id of the group to search for
 * @return the named ImageFrameTimeAxisGroup, or 0 if it is not held upon this view
 */
ImageFrameTimeAxisGroup*
ImageFrameTimeAxisView::get_named_imageframe_group(std::string group_id)
{
	ImageFrameTimeAxisGroup* iftag =  0 ;

	for(ImageFrameGroupList::iterator i = imageframe_groups.begin(); i != imageframe_groups.end(); ++i)
	{
		if (((ImageFrameTimeAxisGroup*)*i)->get_group_name() == group_id)
		{
			iftag = ((ImageFrameTimeAxisGroup*)*i) ;
			break ;
		}
	}

	return(iftag) ;
}


/**
 * Removes and returns the named ImageFrameTimeAxisGroup from the list of ImageFrameTimeAxisGroup held by this view helper
 *
 * @param group_id the ImageFrameTimeAxisGroup unique id to remove
 * @param src the identity of the object that initiated the change
 * @see add_imageframe_group
 */
ImageFrameTimeAxisGroup*
ImageFrameTimeAxisView::remove_named_imageframe_group(std::string group_id, void* src)
{
	ImageFrameTimeAxisGroup* removed = 0 ;

	for(ImageFrameGroupList::iterator iter = imageframe_groups.begin(); iter != imageframe_groups.end(); ++iter)
	{
		if(((ImageFrameTimeAxisGroup*)*iter)->get_group_name() == group_id)
		{
			removed = (*iter) ;
			imageframe_groups.erase(iter) ;

			if(removed == selected_imageframe_group)
			{
				selected_imageframe_group = 0 ;
			}

			 ImageFrameGroupRemoved(removed->get_group_name(), src) ; /* EMIT_SIGNAL */

			// break from the for loop
			break ;
		}
		iter++ ;
	}

	return(removed) ;
}


/**
 * Removes the specified ImageFrameTimeAxisGroup from the list of ImageFrameTimeAxisGroups upon this TimeAxis.
 *
 * @param iftag the ImageFrameView to remove
 */
void
ImageFrameTimeAxisView::remove_imageframe_group(ImageFrameTimeAxisGroup* iftag, void* src)
{
	ENSURE_GUI_THREAD (*this, &ImageFrameTimeAxisView::remove_imageframe_group, iftag, src)

	ImageFrameGroupList::iterator i;
	if((i = find (imageframe_groups.begin(), imageframe_groups.end(), iftag)) != imageframe_groups.end())
	{
		imageframe_groups.erase(i) ;

		 ImageFrameGroupRemoved(iftag->get_group_name(), src) ; /* EMIT_SIGNAL */
	}
}




//---------------------------------------------------------------------------------------//
// Selected group methods

/**
 * Sets the currently selected group upon this time axis
 *
 * @param ifv the item to set selected
 */
void
ImageFrameTimeAxisView::set_selected_imageframe_group(ImageFrameTimeAxisGroup* iftag)
{
	if(selected_imageframe_group)
	{
		selected_imageframe_group->set_selected(false) ;
	}

	selected_imageframe_group = iftag ;
	selected_imageframe_group->set_selected(true) ;
}

/**
 * Clears the currently selected image frame group unpo this time axis
 *
*/
void
ImageFrameTimeAxisView::clear_selected_imageframe_group()
{
	if(selected_imageframe_group)
	{
		selected_imageframe_group->set_selected(false) ;
	}
	selected_imageframe_group = 0 ;
}

/**
 * Returns the currently selected group upon this time axis
 *
 * @return the currently selected group upon this time axis
 */
ImageFrameTimeAxisGroup*
ImageFrameTimeAxisView::get_selected_imageframe_group() const
{
	return(selected_imageframe_group) ;
}

//---------------------------------------------------------------------------------------//
// Selected item methods

/**
 * Sets the currently selected imag frame view item
 *
 * @param iftag the group the selected item is part
 * @param ifv the selected item
 */
void
ImageFrameTimeAxisView::set_selected_imageframe_view(ImageFrameTimeAxisGroup* iftag, ImageFrameView* ifv)
{
	set_selected_imageframe_group(iftag) ;

	if(selected_imageframe_view)
	{
		selected_imageframe_view->set_selected(false) ;
	}

	selected_imageframe_view = ifv ;
	selected_imageframe_view->set_selected(true) ;
}

/**
 * Clears the currently selected image frame view item
 *
 */
void
ImageFrameTimeAxisView::clear_selected_imageframe_item(bool clear_group)
{
	if(clear_group)
	{
		clear_selected_imageframe_group() ;
	}

	if(selected_imageframe_view)
	{
		selected_imageframe_view->set_selected(false) ;
	}
	selected_imageframe_view = 0 ;
}

/**
 * Returns the currently selected image frame view item upon this time axis
 *
 * @return the currently selected image frame view item
 */
ImageFrameView*
ImageFrameTimeAxisView::get_selected_imageframe_view() const
{
	return(selected_imageframe_view) ;
}




void
ImageFrameTimeAxisView::set_imageframe_duration_sec(double sec)
{
	if (selected_imageframe_group && selected_imageframe_view) {
		selected_imageframe_view->set_duration ((sec * _trackview.editor.session()->frame_rate()), this);
	}
}



/**
 * Removes the currently selected ImageFrame view item
 *
 * @param src the identity of the object that initiated the change
 * @see add_imageframe_group
 */
void
ImageFrameTimeAxisView::remove_selected_imageframe_item(void* src)
{
	if(selected_imageframe_group && selected_imageframe_view)
	{
		ImageFrameView* temp_item = selected_imageframe_view ;
		selected_imageframe_group->remove_imageframe_item(temp_item, src) ;

		// XXX although we have removed the item from the group, we need the group id still set within the
		//     item as the remove method requires this data when telling others about the deletion
		//     to fully specify the item we need the track, group and item id
		selected_imageframe_view->remove_this_item(src) ;
		clear_selected_imageframe_item(false) ;
	}
}

