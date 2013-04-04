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

#ifndef __ardour_visual_time_axis_h__
#define __ardour_visual_time_axis_h__

#include <gtkmm/box.h>
#include <gtkmm/button.h>

#include <gtkmm2ext/selector.h>

#include "ardour_dialog.h"
#include "route_ui.h"
#include "enums.h"
#include "time_axis_view.h"

namespace ARDOUR
{
	class Session ;
}

class PublicEditor;
class ImageFrameView;
class ImageFrameTimeAxisView;
class MarkersTimeAxisView;
class TimeSelection;
class RegionSelection;
class MarkerTimeAxis;
class TimeAxisViewStrip;

/**
 * Base Abstact Class for TimeAxis views that operate purely within the visual domain.
 *
 * This class provides many of the common methods required for visual TimeAxis views. The aim is
 * to provide an abstract layer during the developmnt of the visual based time axis'. Many of these
 * methods have a better home further up the class heirarchy, and in fact some are replication of
 * methods found within RouteUI. This, however, has been required due to various problems with previous
 * versions of g++, mainly 2.95, which are not correctly handling virtual methods, virtual base classes,
 * and virtual methods when used with Multiple Inheritance. Perhaps these could be combined once the
 * compilers all agree on hos do to do stuff...
 */
class VisualTimeAxis : public TimeAxisView
{
	public:
		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * VisualTimeAxis Destructor
		 *
		 */
		virtual ~VisualTimeAxis() ;

		//---------------------------------------------------------------------------------------//
		// Name/Id Accessors/Mutators

		/**
		 * Returns the name of this TimeAxis
		 *
		 * @return the name of this TimeAxis
		 */
		virtual std::string name() const ;

		/**
		 * Sets the name of this TimeAxis
		 *
		 * @param name the new name of this TimeAxis
		 * @param src the identity of the object that initiated the change
		 */
		virtual void set_time_axis_name(const std::string & name, void* src) ;


		//---------------------------------------------------------------------------------------//
		// ui methods & data

		/**
		 * Sets the height of this TrackView to one of the defined TrackHeghts
		 *
		 * @param h the number of pixels to set the height to
		 */
		virtual void set_height (uint32_t);

		//---------------------------------------------------------------------------------------//
		// Selection Methods
		// selection methods are not handled by visual time axis object yet...

		/**
		 * Not implemented
		 */
		virtual void set_selected_regionviews(RegionSelection&) ;


		//---------------------------------------------------------------------------------//
		// Emitted Signals

		/**
		 * Emitted when we have changed the gui, and what we have shanged
		 */
		sigc::signal<void,const std::string &,void*> gui_changed ;

		/**
		 * Emitted when this Visual Time Axis has been removed
		 * This is different to the CatchDeletion signal in that this signal
		 * is emitted during the deletion of this Time Axis, and not during
		 * the destructor, this allows us to capture the source of the deletion
		 * event
		 */
		sigc::signal<void,const std::string &,void*> VisualTimeAxisRemoved ;

		/**
		 * Emitted when we have changed the name of this TimeAxis
		 */
		sigc::signal<void,const std::string &,const std::string &,void*> NameChanged ;

		/**
		 * Emitted when this time axis has been selected for removal
		 */
		//sigc::signal<void,std::std::string,void*> VisualTimeAxisRemoved ;

		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * Abstract Constructor for base visual time axis classes
		 *
		 * @param name the name/Id of thie TimeAxis
		 * @param ed the Ardour PublicEditor
		 * @param sess the current session
		 * @param canvas the parent canvas object
		 */
		VisualTimeAxis(const std::string & name, PublicEditor& ed, ARDOUR::Session* sess, ArdourCanvas::Canvas& canvas) ;


		//---------------------------------------------------------------------------------------//
		// Handle time axis removal

		/**
		 * Handles the Removal of this VisualTimeAxis
		 *
		 * @param src the identity of the object that initiated the change
		 */
		virtual void remove_this_time_axis(void* src) ;

		/**
		 * Callback used to remove this time axis during the gtk idle loop
		 * This is used to avoid deleting the obejct while inside the remove_this_time_axis
		 * method
		 *
		 * @param ta the VisualTimeAxis to remove
		 * @param src the identity of the object that initiated the change
		 */
		static gint idle_remove_this_time_axis(VisualTimeAxis* ta, void* src) ;



		//---------------------------------------------------------------------------------------//
		// ui methods & data

		/**
		 * Handle the visuals button click
		 *
		 */
		void visual_click() ;

		/**
		 * Handle the hide buttons click
		 *
		 */
		void hide_click() ;

		/**
		 * Allows the selection of a new color for this TimeAxis
		 *
		 */
		virtual void select_track_color() ;

		/**
		 * Provides a color chooser for the selection of a new time axis color.
		 *
		 */
		 bool choose_time_axis_color() ;

		/**
		 * Sets the color of this TimeAxis to the specified color c
		 *
		 * @param c the new TimeAxis color
		 */
		void set_time_axis_color(Gdk::Color c) ;


		//---------------------------------------------------------------------------------------//
		// Handle TimeAxis rename

		/**
		 * Construct a new prompt to receive a new name for this TimeAxis
		 *
		 * @see finish_time_axis_rename()
		 */
		void start_time_axis_rename() ;

		/**
		 * Handles the new name for this TimeAxis from the name prompt
		 *
		 * @see start_time_axis_rename()
		 */
		virtual void label_view() ;


		//---------------------------------------------------------------------------------------//
		// Handle name entry signals

		void name_entry_changed() ;
		bool name_entry_key_release_handler(GdkEventKey*) ;

		//---------------------------------------------------------------------------------------//
		// VisualTimeAxis Widgets
		Gtk::HBox other_button_hbox ;
		Gtk::Button hide_button ;
		Gtk::Button visual_button ;
		Gtk::Button size_button ;

		/** the name of this TimeAxis object */
		std::string time_axis_name ;

		//---------------------------------------------------------------------------------------//
		// Super class methods not handled by VisualTimeAxis

		/**
		 * Not handled by purely Visual TimeAxis
		 *
		 * @todo should VisualTimeAxis handle this?
		 */
		void show_timestretch (nframes_t start, nframes_t end, int layers, int layer);

		/**
		 * Not handle by purely visual TimeAxis
		 * @see show_timestratch
		 */
		virtual void hide_timestretch() ;

	private:

};

#endif /* __ardour_visual_time_axis_h__ */

