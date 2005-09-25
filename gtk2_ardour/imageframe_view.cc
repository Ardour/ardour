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

    $Id$
*/

#include <algorithm>
#include <cmath>

#include <gtk--.h>
#include <gtkmmext/gtk_ui.h>

#include "imageframe_time_axis.h"
#include "imageframe_time_axis_group.h"
#include "marker_time_axis.h"
#include "marker_time_axis_view.h"
#include "canvas-simplerect.h"
#include "public_editor.h"
#include "utils.h"
#include "imageframe_view.h"
#include "canvas-imageframe.h"
#include "gui_thread.h"

using namespace SigC ;
using namespace ARDOUR ;

SigC::Signal1<void,ImageFrameView*> ImageFrameView::GoingAway;

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
ImageFrameView::ImageFrameView(std::string item_id,
	GtkCanvasGroup *parent,
	ImageFrameTimeAxis* tv,
	ImageFrameTimeAxisGroup* item_group,
	double spu,
	GdkColor& basic_color,
	jack_nframes_t start,
	jack_nframes_t duration,
	unsigned char* rgb_data,
	uint32_t width,
	uint32_t height,
	uint32_t num_channels)
  : TimeAxisViewItem(item_id, parent, *tv, spu, basic_color, start, duration,
		     TimeAxisViewItem::Visibility (TimeAxisViewItem::ShowNameText|
						   TimeAxisViewItem::ShowNameHighlight|
						   TimeAxisViewItem::ShowFrame|
						   TimeAxisViewItem::ShowHandles))

{
	the_parent_group = item_group ;
	set_name_text(item_id) ;
	
	image_data_width = width ;
	image_data_height = height ;
	image_data_num_channels = num_channels ;
	
	//This should be art_free'd once the ArtPixBuf is destroyed - this should happen when we destroy the imageframe canvas item
	unsigned char* the_rgb_data = (unsigned char*) art_alloc(width*height*num_channels) ;
	memcpy(the_rgb_data, rgb_data, (width*height*num_channels)) ;

	ArtPixBuf* pbuf ;
	pbuf = art_pixbuf_new_rgba(the_rgb_data, width, height, (num_channels * width));
	imageframe = 0 ;
	
	//calculate our image width based on the track height
	double im_ratio = (double)width/(double)height ;
	int im_width = (int)((double)(trackview.height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE) * im_ratio) ;
	
	imageframe = gtk_canvas_item_new(GTK_CANVAS_GROUP(group),
		gtk_canvas_imageframe_get_type(),
		"pixbuf", pbuf,
		"x", (gdouble) 1.0,
		"y", (gdouble) 1.0,
		"anchor", GTK_ANCHOR_NW,
		"width", (gdouble) im_width,
		"height", (gdouble) (trackview.height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE),
		NULL) ;


	gtk_signal_connect (GTK_OBJECT(frame_handle_start), "event",
						(GtkSignalFunc) PublicEditor::canvas_imageframe_start_handle_event,
						this);
	
	gtk_signal_connect (GTK_OBJECT(frame_handle_end), "event",
						(GtkSignalFunc) PublicEditor::canvas_imageframe_end_handle_event,
						this);
						
	gtk_signal_connect (GTK_OBJECT(group), "event",
		(GtkSignalFunc) PublicEditor::canvas_imageframe_item_view_event, this); 

	/* handle any specific details required by the initial start end duration values */
	
	gtk_canvas_item_raise_to_top(frame_handle_start) ;
	gtk_canvas_item_raise_to_top(frame_handle_end) ;
	
    set_position(start, this) ;
    set_duration(duration, this) ;
}

/**
 * Destructor
 * Reposible for removing and destroying all marker items associated with this item
 */
ImageFrameView::~ImageFrameView()
{
	GoingAway (this);

	// destroy any marker items we have associated with this item
		
	for(MarkerViewList::iterator iter = marker_view_list.begin(); iter != marker_view_list.end(); ++iter)
	{
		MarkerView* mv = (*iter) ;
		
		MarkerViewList::iterator next = iter ;
		next++ ;

		// remove the item from our marker list
		// the current iterator becomes invalid after this point, so we cannot call next upon it
		// luckily enough, we already have next
		marker_view_list.erase(iter) ;

		// remove the item from the marker time axis
		MarkerTimeAxisView* mtav = dynamic_cast<MarkerTimeAxis*>(&mv->get_time_axis_view())->get_view() ;
		if(mtav)
		{
			mtav->remove_marker_view(mv, this) ;
		}

		mv->set_marked_item(0) ;
		delete mv ;
		mv = 0 ;
		
		// set our iterator to next, as we have invalided the current iterator with the call to erase 
		iter = next ;
	}
	
	// if we are the currently selected item withi the parent track, we need to se-select
	if(the_parent_group)
	{
		if(the_parent_group->get_view().get_selected_imageframe_view() == this)
		{
			the_parent_group->get_view().clear_selected_imageframe_item(false) ;
		}
	}

	if(imageframe)
	{
		gtk_object_destroy(GTK_OBJECT(imageframe)) ;
		imageframe = 0 ;
	}
}


//---------------------------------------------------------------------------------------//
// Position and duration Accessors/Mutators

/**
 * Set the position of this item to the specified value
 *
 * @param pos the new position
 * @param src the identity of the object that initiated the change
 * @return true if the position change was a success, false otherwise
 */
bool
ImageFrameView::set_position(jack_nframes_t pos, void* src)
{
	jack_nframes_t old_pos = frame_position ;
	
	// do the standard stuff
	bool ret = TimeAxisViewItem::set_position(pos, src) ;

	// everything went ok with the standard stuff?
	if(ret)
	{
		/* move each of our associated markers with this ImageFrameView */
		for (MarkerViewList::iterator i = marker_view_list.begin(); i != marker_view_list.end(); ++i)
		{
			// calculate the offset of the marker
			MarkerView* mv = (MarkerView*)*i ;
			jack_nframes_t marker_old_pos = mv->get_position() ;
			
			mv->set_position(pos + (marker_old_pos - old_pos), src) ;
		}
	}
	
	return(ret) ;
}
		 
/**
 * Sets the duration of this item
 *
 * @param dur the new duration of this item
 * @param src the identity of the object that initiated the change
 * @return true if the duration change was succesful, false otherwise
 */
bool
ImageFrameView::set_duration(jack_nframes_t dur, void* src)
{
	/* do the standard stuff */
	bool ret = TimeAxisViewItem::set_duration(dur, src) ;
	
	// eveything went ok with the standard stuff?
	if(ret)
	{
		/* handle setting the sizes of our canvas itesm based on the new duration */
		gtk_canvas_item_set(imageframe, "drawwidth", (gdouble) trackview.editor.frame_to_pixel(get_duration()), NULL) ;
	}
	
	return(ret) ;
}

//---------------------------------------------------------------------------------------//
// Parent Component Methods
		
/**
 * Sets the parent ImageFrameTimeAxisGroup of thie item
 * each Item must be part of exactly one group (or 'scene') upon the timeline
 *
 * @param group the new parent group
 */
void
ImageFrameView::set_time_axis_group(ImageFrameTimeAxisGroup* group)
{
	the_parent_group = group ;
}
		
/**
 * Returns the parent group of this item
 *
 * @return the parent group of this item
 */
ImageFrameTimeAxisGroup*
ImageFrameView::get_time_axis_group()
{
	return(the_parent_group) ;
}


//---------------------------------------------------------------------------------------//
// ui methods
		
/**
 * Set the height of this item
 *
 * @param h the new height
 */
void
ImageFrameView::set_height (gdouble h)
{
	// set the image size
	// @todo might have to re-get the image data, for a large height...hmmm.
	double im_ratio = (double)image_data_width/(double)image_data_height ;
	int im_width = (int)((double)(h - TimeAxisViewItem::NAME_Y_OFFSET) * im_ratio) ;
	gtk_canvas_item_set(imageframe, "width", (gdouble)im_width, NULL) ;
	gtk_canvas_item_set(imageframe, "height",(gdouble) (h - TimeAxisViewItem::NAME_Y_OFFSET), NULL) ;

	
	gtk_canvas_item_raise_to_top(frame) ;
	gtk_canvas_item_raise_to_top(imageframe) ;
	gtk_canvas_item_raise_to_top(name_highlight) ;
	gtk_canvas_item_raise_to_top(name_text) ;
	gtk_canvas_item_raise_to_top(frame_handle_start) ;
	gtk_canvas_item_raise_to_top(frame_handle_end) ;
 
 	gtk_canvas_item_set (name_text, "y", h - TimeAxisViewItem::NAME_Y_OFFSET, NULL);
	gtk_canvas_item_set (frame, "y2", h, NULL);

	gtk_canvas_item_set (name_highlight, "y1", (gdouble) h - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE, "y2", (gdouble) h - 1.0, NULL);
}


//---------------------------------------------------------------------------------------//
// MarkerView methods

/**
 * Adds a markerView to the list of marker views associated with this item
 *
 * @param item the marker item to add
 * @param src the identity of the object that initiated the change
 */
void
ImageFrameView::add_marker_view_item(MarkerView* item, void* src)
{
	marker_view_list.push_back(item) ;
	
	item->GoingAway.connect(bind(slot(*this, &ImageFrameView::remove_marker_view_item), (void*)this));
	
	 MarkerViewAdded(item, src) ; /* EMIT_SIGNAL */
}
		
/**
 * Removes the named marker view from the list of marker view associated with this item
 * The Marker view is not destroyed on removal, so the caller must handle the item themself
 *
 * @param markId the id/name of the item to remove
 * @param src the identity of the object that initiated the change
 * @return the removed marker item
 */
MarkerView*
ImageFrameView::remove_named_marker_view_item(std::string markerId, void* src)
{
	MarkerView* mv = 0 ;
	MarkerViewList::iterator i = marker_view_list.begin() ;
	
	while(i != marker_view_list.end())
	{
		if (((MarkerView*)*i)->get_item_name() == markerId)
		{
			mv = (*i) ;

			marker_view_list.erase(i) ;
			
			 MarkerViewRemoved(mv,src) ; /* EMIT_SIGNAL */
			
			// iterator is now invalid, but since we should only ever have
			// one item with the specified name, things are ok, and we can
			// break from the while loop
			break ;
		}
		i++ ;
	}
	
	return(mv) ;
}
		
/**
 * Removes item from the list of marker views assocaited with this item
 * This method will do nothing if item if not assiciated with this item
 *
 * @param item the item to remove
 * @param src the identity of the object that initiated the change
 */
void
ImageFrameView::remove_marker_view_item(MarkerView* mv, void* src)
{
	ENSURE_GUI_THREAD(bind (slot (*this, &ImageFrameView::remove_marker_view_item), mv, src));

	MarkerViewList::iterator i ;
	
	if((i = find (marker_view_list.begin(), marker_view_list.end(), mv)) != marker_view_list.end()) {
		marker_view_list.erase(i) ;
		 MarkerViewRemoved (mv, src) ; /* EMIT_SIGNAL */
	}
}
		
/**
 * Determines if the named marker is one of those associated with this item
 *
 * @param markId the id/name of the item to search for
 */
bool
ImageFrameView::has_marker_view_item(std::string mname)
{
	bool result = false ;
	
	for (MarkerViewList::iterator ci = marker_view_list.begin(); ci != marker_view_list.end(); ++ci)
	{
		if (((MarkerView*)*ci)->get_item_name() == mname)
		{
			result = true ;
			
			// found the item, so we can break the for loop
			break ;
		}
	}
	
	return(result) ;
}
