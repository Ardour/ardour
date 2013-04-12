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

#include "pbd/error.h"

#include <gtkmm/menu.h>

#include <gtkmm2ext/utils.h>

#include "ardour/session.h"
#include "ardour/utils.h"

#include "ardour_ui.h"
#include "public_editor.h"
#include "imageframe_time_axis.h"
#include "selection.h"
#include "imageframe_time_axis_view.h"
#include "marker_time_axis_view.h"
#include "imageframe_view.h"
#include "marker_time_axis.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

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
MarkerTimeAxis::MarkerTimeAxis (PublicEditor& ed, ARDOUR::Session* sess, Canvas& canvas, const string & name, TimeAxisView* tav)
	: AxisView(sess),
	  VisualTimeAxis(name, ed, sess, canvas)
{
	/* the TimeAxisView these markers are associated with */
	marked_time_axis = tav ;

	_color = unique_random_color() ;
	time_axis_name = name ;

	selection_group = new Group (*canvas_display);
	selection_group->hide();

	// intialize our data items
	marker_menu = 0 ;

	y_position = -1 ;

	/* create our new marker time axis strip view */
	view = new MarkerTimeAxisView(*this) ;

	// set the initial time axis text label
	label_view() ;

	// set the initial height of this time axis
	set_height(hSmall) ;
}

/**
 * Destructor
 * Responsible for destroying any marker items upon this time axis
 */
MarkerTimeAxis::~MarkerTimeAxis()
{
	CatchDeletion (this); /* EMIT_SIGNAL */

	// destroy the view helper
	// this handles removing and destroying individual marker items

	if(view) {
		delete view ;
		view = 0 ;
	}
}


//---------------------------------------------------------------------------------------//
// ui methods & data

void
MarkerTimeAxis::set_height (uint32_t h)
{
	VisualTimeAxis::set_height(h) ;

	// tell out view helper of the change too
	if (view != 0)
	{
		view->set_height((double) height) ;
	}

	// tell those interested that we have had our height changed
	gui_changed("track_height",(void*)0) ; /* EMIT_SIGNAL */
}

/**
 * Sets the number of frames per pixel that are used.
 * This is used to determine the sizes of items upon this time axis
 *
 * @param spu the number of frames per pixel
 */
void
MarkerTimeAxis::set_samples_per_pixel (double fpp)
{
	TimeAxisView::set_samples_per_pixel (editor.get_current_zoom());

	if (view) {
		view->set_samples_per_pixel (fpp);
	}
}

/**
 * Show the popup edit menu
 *
 * @param button the mouse button pressed
 * @param time when to show the popup
 * @param clicked_mv the MarkerView that the event ocured upon, or 0 if none
 * @param with_item true if an item has been selected upon the time axis, used to set context menu
 */
void
MarkerTimeAxis::popup_marker_time_axis_edit_menu(int button, int32_t time, MarkerView* clicked_mv, bool with_item)
{
	if (!marker_menu)
	{
		build_marker_menu() ;
	}

	if (with_item)
	{
		marker_item_menu->set_sensitive(true) ;
	}
	else
	{
		marker_item_menu->set_sensitive(false) ;
	}

	marker_menu->popup(button,time) ;
}


/**
 * convenience method to select a new track color and apply it to the view and view items
 *
 */
void
MarkerTimeAxis::select_track_color()
{
	if(VisualTimeAxis::choose_time_axis_color())
	{
		if(view)
		{
			view->apply_color(_color) ;
		}
	}
}

/**
 * Handles the building of the popup menu
 */
void
MarkerTimeAxis::build_display_menu()
{
	using namespace Menu_Helpers;

	/* get the size menu ready */
	build_size_menu() ;

	/* prepare it */
	TimeAxisView::build_display_menu();

	/* now fill it with our stuff */
	MenuList& items = display_menu->items();

	items.push_back(MenuElem (_("Rename"), sigc::mem_fun(*this, &VisualTimeAxis::start_time_axis_rename)));

	items.push_back(SeparatorElem()) ;
	items.push_back(MenuElem (_("Height"), *size_menu));
	items.push_back(MenuElem (_("Color"), sigc::mem_fun(*this, &MarkerTimeAxis::select_track_color)));
	items.push_back(SeparatorElem()) ;

	items.push_back(MenuElem (_("Remove"), sigc::bind(sigc::mem_fun(*this, &MarkerTimeAxis::remove_this_time_axis), (void*)this)));
}

/**
 * handles the building of the MarkerView sub menu
 */
void
MarkerTimeAxis::build_marker_menu()
{
	using namespace Menu_Helpers;
	using Gtk::Menu;

	marker_menu = manage(new Menu) ;
	marker_menu->set_name ("ArdourContextMenu");
	MenuList& items = marker_menu->items();

	marker_item_menu = manage(new Menu) ;
	marker_item_menu->set_name ("ArdourContextMenu");
	MenuList& marker_sub_items = marker_item_menu->items() ;

	/* duration menu */
	Menu* duration_menu = manage(new Menu) ;
	duration_menu->set_name ("ArdourContextMenu");
	MenuList& duration_items = duration_menu->items() ;

	if(view)
	{
		duration_items.push_back(MenuElem (_("1 seconds"), sigc::bind (sigc::mem_fun (view, &MarkerTimeAxisView::set_marker_duration_sec), 1.0))) ;
		duration_items.push_back(MenuElem (_("1.5 seconds"), sigc::bind (sigc::mem_fun (view, &MarkerTimeAxisView::set_marker_duration_sec), 1.5))) ;
		duration_items.push_back(MenuElem (_("2 seconds"), sigc::bind (sigc::mem_fun (view, &MarkerTimeAxisView::set_marker_duration_sec), 2.0))) ;
		duration_items.push_back(MenuElem (_("2.5 seconds"), sigc::bind (sigc::mem_fun (view, &MarkerTimeAxisView::set_marker_duration_sec), 2.5))) ;
		duration_items.push_back(MenuElem (_("3 seconds"), sigc::bind (sigc::mem_fun (view, &MarkerTimeAxisView::set_marker_duration_sec), 3.0))) ;
	}
	//duration_items.push_back(SeparatorElem()) ;
	//duration_items.push_back(MenuElem (_("custom"), sigc::mem_fun(*this, &ImageFrameTimeAxis::set_marker_duration_custom))) ;

	marker_sub_items.push_back(MenuElem(_("Duration (sec)"), *duration_menu)) ;

	marker_sub_items.push_back(SeparatorElem()) ;
	marker_sub_items.push_back(MenuElem (_("Remove Marker"), sigc::bind(sigc::mem_fun(view, &MarkerTimeAxisView::remove_selected_marker_view),(void*)this))) ;

	items.push_back(MenuElem(_("Marker"), *marker_item_menu)) ;
	items.push_back(MenuElem (_("Rename Track"), sigc::mem_fun(*this,&MarkerTimeAxis::start_time_axis_rename))) ;

	marker_menu->show_all() ;
}



/**
 * Returns the view helper of this TimeAxis
 *
 * @return the view helper of this TimeAxis
 */
MarkerTimeAxisView*
MarkerTimeAxis::get_view()
{
	return(view) ;
}

/**
 * Returns the TimeAxisView that this markerTimeAxis is marking up
 *
 * @return the TimeAXisView that this MarkerTimeAxis is marking
 */
TimeAxisView*
MarkerTimeAxis::get_marked_time_axis()
{
	return(marked_time_axis) ;
}




