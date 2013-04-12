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

#include <string>
#include <algorithm>

#include "pbd/error.h"

#include <gtkmm/menu.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/gtk_ui.h>

#include "ardour/session.h"
#include "ardour/utils.h"

#include "public_editor.h"
#include "imageframe_time_axis.h"
#include "enums.h"
#include "imageframe_time_axis_view.h"
#include "imageframe_time_axis_group.h"
#include "marker_time_axis_view.h"
#include "imageframe_view.h"
#include "marker_time_axis.h"
#include "marker_view.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

/**
 * Constructs a new ImageFrameTimeAxis.
 *
 * @param track_id the track name/id
 * @param ed the PublicEditor
 * @param sess the current session
 * @param canvas the parent canvas item
 */
ImageFrameTimeAxis::ImageFrameTimeAxis(const string & track_id, PublicEditor& ed, ARDOUR::Session* sess, ArdourCanvas::Canvas& canvas)
	: AxisView(sess),
	  VisualTimeAxis(track_id, ed, sess, canvas)
{
	_color = unique_random_color() ;

	selection_group = new ArdourCanvas::Group (*canvas_display);
	selection_group->hide();

	// intialize our data items
	y_position = -1 ;

	/* create our new image frame view */
	view = new ImageFrameTimeAxisView(*this) ;

	/* create the Image Frame Edit Menu */
	create_imageframe_menu() ;

	// set the initial time axis text label
	label_view() ;

	// set the initial height of this time axis
	set_height(hNormal) ;

	TimeAxisView::CatchDeletion.connect (*this, boost::bind (&ImageFrameTimeAxis::remove_time_axis_view, this, _1), gui_context());
}

/**
 * Destructor
 * Responsible for destroying any child image items that may have been added to thie time axis
 */
ImageFrameTimeAxis::~ImageFrameTimeAxis ()
{
	CatchDeletion (this);

	// Destroy all the marker views we may have associaited with this TimeAxis
	for(MarkerTimeAxisList::iterator iter = marker_time_axis_list.begin(); iter != marker_time_axis_list.end(); ++iter)
	{
		MarkerTimeAxis* mta = *iter ;
		MarkerTimeAxisList::iterator next = iter ;
		next++ ;

		marker_time_axis_list.erase(iter) ;

		delete mta ;
		mta = 0 ;

		iter = next ;
	}

	delete image_action_menu ;
	image_action_menu = 0 ;

	delete selection_group;
	selection_group = 0 ;

	// Destroy our Axis View helper
	delete view ;
	view = 0 ;
}

//---------------------------------------------------------------------------------------//
// ui methods & data

/**
 * Sets the height of this TrackView to one of ths TrackHeghts
 *
 * @param h
 */
void
ImageFrameTimeAxis::set_height (uint32_t h)
{
	VisualTimeAxis::set_height(h) ;

	// tell out view helper of the change too
	if(view != 0)
	{
		view->set_height((double) height) ;
	}

	// tell those interested that we have had our height changed
	gui_changed("track_height",(void*)0); /* EMIT_SIGNAL */
}

/**
 * Sets the number of frames per pixel that are used.
 * This is used to determine the siezes of items upon this time axis
 *
 * @param fpp the number of frames per pixel
 */
void
ImageFrameTimeAxis::set_samples_per_pixel (double fpp)
{
	TimeAxisView::set_samples_per_pixel (editor.get_current_zoom ());

	if (view) {
		view->set_samples_per_pixel (fpp);
	}
}


/**
 * Returns the available height for images to be drawn onto
 *
 * @return the available height for an image item to be drawn onto
 */
int
ImageFrameTimeAxis::get_image_display_height()
{
	return(height - (gint)TimeAxisViewItem::NAME_HIGHLIGHT_SIZE) ;
}


/**
 * Show the popup edit menu
 *
 * @param button the mouse button pressed
 * @param time when to show the popup
 * @param clicked_imageframe the ImageFrameItem that the event ocured upon, or 0 if none
 * @param with_item true if an item has been selected upon the time axis, used to set context menu
 */
void
ImageFrameTimeAxis::popup_imageframe_edit_menu(int button, int32_t time, ImageFrameView* clicked_imageframe, bool with_item)
{
	if (!imageframe_menu)
	{
		create_imageframe_menu() ;
	}

	if(with_item)
	{
		imageframe_item_menu->set_sensitive(true) ;
	}
	else
	{
		imageframe_item_menu->set_sensitive(false) ;
	}

	imageframe_menu->popup(button,time) ;
}

/**
 * convenience method to select a new track color and apply it to the view and view items
 *
 */
void
ImageFrameTimeAxis::select_track_color()
{
	if (choose_time_axis_color())
	{
		if (view)
		{
			view->apply_color (_color) ;
		}
	}
}

/**
 * Handles the building of the popup menu
 */
void
ImageFrameTimeAxis::build_display_menu()
{
	using namespace Menu_Helpers;
	using Gtk::Menu;

	/* get the size menu ready */

	build_size_menu();

	/* prepare it */

	TimeAxisView::build_display_menu () ;

	/* now fill it with our stuff */

	MenuList& items = display_menu->items();

	items.push_back (MenuElem (_("Rename"), sigc::mem_fun(*this, &ImageFrameTimeAxis::start_time_axis_rename)));

	image_action_menu = new Menu() ;
	image_action_menu->set_name ("ArdourContextMenu");
	MenuList image_items = image_action_menu->items() ;

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Height"), *size_menu));
	items.push_back (MenuElem (_("Color"), sigc::mem_fun(*this, &ImageFrameTimeAxis::select_track_color)));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::bind(sigc::mem_fun(*this, &VisualTimeAxis::remove_this_time_axis), (void*)this))) ;
}

/**
 * handles the building of the ImageFrameView sub menu
 */
void
ImageFrameTimeAxis::create_imageframe_menu()
{
	using namespace Menu_Helpers;
	using Gtk::Menu;

	imageframe_menu = manage(new Menu) ;
	imageframe_menu->set_name ("ArdourContextMenu");
	MenuList& items = imageframe_menu->items();

	imageframe_item_menu = manage(new Menu) ;
	imageframe_item_menu->set_name ("ArdourContextMenu");
	MenuList& imageframe_sub_items = imageframe_item_menu->items() ;

	/* duration menu */
	Menu* duration_menu = manage(new Menu) ;
	duration_menu->set_name ("ArdourContextMenu");
	MenuList& duration_items = duration_menu->items() ;

	if(view)
	{
		duration_items.push_back(MenuElem (_("0.5 seconds"), sigc::bind (sigc::mem_fun (view, &ImageFrameTimeAxisView::set_imageframe_duration_sec), 0.5))) ;
		duration_items.push_back(MenuElem (_("1 seconds"), sigc::bind (sigc::mem_fun (view, &ImageFrameTimeAxisView::set_imageframe_duration_sec), 1.0))) ;
		duration_items.push_back(MenuElem (_("1.5 seconds"), sigc::bind (sigc::mem_fun (view, &ImageFrameTimeAxisView::set_imageframe_duration_sec), 1.5))) ;
		duration_items.push_back(MenuElem (_("2 seconds"), sigc::bind (sigc::mem_fun (view, &ImageFrameTimeAxisView::set_imageframe_duration_sec), 2.0))) ;
		duration_items.push_back(MenuElem (_("2.5 seconds"), sigc::bind (sigc::mem_fun (view, &ImageFrameTimeAxisView::set_imageframe_duration_sec), 2.5))) ;
		duration_items.push_back(MenuElem (_("3 seconds"), sigc::bind (sigc::mem_fun (view, &ImageFrameTimeAxisView::set_imageframe_duration_sec), 3.0))) ;
		//duration_items.push_back(SeparatorElem()) ;
		//duration_items.push_back(MenuElem (_("custom"), sigc::mem_fun(*this, &ImageFrameTimeAxis::set_imageframe_duration_custom))) ;
	}

	imageframe_sub_items.push_back(MenuElem(_("Duration (sec)"), *duration_menu)) ;

	imageframe_sub_items.push_back(SeparatorElem()) ;
	if(view)
	{
		imageframe_sub_items.push_back(MenuElem (_("Remove Frame"), sigc::bind(sigc::mem_fun (view, &ImageFrameTimeAxisView::remove_selected_imageframe_item), (void*)this))) ;
	}

	items.push_back(MenuElem(_("Image Frame"), *imageframe_item_menu)) ;
	items.push_back(MenuElem (_("Rename Track"), sigc::mem_fun(*this,&ImageFrameTimeAxis::start_time_axis_rename))) ;

	imageframe_menu->show_all() ;
}




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
bool
ImageFrameTimeAxis::add_marker_time_axis(MarkerTimeAxis* marker_track, void* src)
{
	bool ret = false ;

	if(get_named_marker_time_axis(marker_track->name()) != 0)
	{
		ret = false ;
	}
	else
	{
		marker_time_axis_list.push_back(marker_track) ;
		 MarkerTimeAxisAdded(marker_track, src) ; /* EMIT_SIGNAL */
		ret = true ;
	}

	return(ret) ;
}

/**
 * Returns the named MarkerTimeAxis associated with this ImageFrameTimeAxis
 *
 * @param track_id the track_id of the MarkerTimeAxis to search for
 * @return the named markerTimeAxis, or 0 if the named MarkerTimeAxis is not associated with this ImageFrameTimeAxis
 */
MarkerTimeAxis*
ImageFrameTimeAxis::get_named_marker_time_axis(const string & track_id)
{
	MarkerTimeAxis* mta =  0 ;

	for (MarkerTimeAxisList::iterator i = marker_time_axis_list.begin(); i != marker_time_axis_list.end(); ++i)
	{
		if (((MarkerTimeAxis*)*i)->name() == track_id)
		{
			mta = ((MarkerTimeAxis*)*i) ;
			break ;
		}
	}
	return(mta) ;
}

/**
 * Removes the named markerTimeAxis from those associated with this ImageFrameTimeAxis
 *
 * @param track_id the track id of the MarkerTimeAxis to remove
 * @param src the identity of the object that initiated the change
 * @return the removed MarkerTimeAxis
 */
MarkerTimeAxis*
ImageFrameTimeAxis::remove_named_marker_time_axis(const string & track_id, void* src)
{
	MarkerTimeAxis* mta = 0 ;

	for(MarkerTimeAxisList::iterator i = marker_time_axis_list.begin(); i != marker_time_axis_list.end(); ++i)
	{
		if (((MarkerTimeAxis*)*i)->name() == track_id)
		{
			mta = ((MarkerTimeAxis*)*i) ;

			// the iterator is invalid after this call, so we can no longer use it as is.
			marker_time_axis_list.erase(i) ;

			 MarkerTimeAxisRemoved(mta->name(), src) ; /* EMIT_SIGNAL */
			break ;
		}
	}

	return(mta) ;
}

/**
 * Removes the specified MarkerTimeAxis from the list of MarkerTimaAxis associated with this ImageFrameTimeAxis
 * Note that the MarkerTimeAxis is not deleted, only removed from the list os associated tracks
 *
 * @param mta the TimeAxis to remove
 * @param src the identity of the object that initiated the change
 */
void
ImageFrameTimeAxis::remove_time_axis_view (TimeAxisView* tav)
{
	MarkerTimeAxisView* mtav = dynamic_cast<MarkerTimeAxisView*> (tav);

	if (!mtav) {
		return;
	}

	MarkerTimeAxisList::iterator i;

	if ((i = find (marker_time_axis_list.begin(), marker_time_axis_list.end(), mta)) != marker_time_axis_list.end())  {
		// note that we dont delete the object itself, we just remove it from our list
		marker_time_axis_list.erase(i) ;
		MarkerTimeAxisRemoved (mta->name(), src) ; /* EMIT_SIGNAL */
	}
}


//---------------------------------------------------------------------------------------//
// Parent/Child helper object accessors

/**
 * Returns the view helper of this TimeAxis
 *
 * @return the view helper of this TimeAxis
 */
ImageFrameTimeAxisView*
ImageFrameTimeAxis::get_view()
{
	return(view) ;
}
