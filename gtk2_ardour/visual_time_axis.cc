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

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

#include <pbd/error.h>
#include <pbd/stl_delete.h>
#include <pbd/whitespace.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/choice.h>

#include <ardour/session.h>
#include <ardour/utils.h>
#include <ardour/insert.h>
#include <ardour/location.h>

#include "ardour_ui.h"
#include "public_editor.h"
#include "imageframe_time_axis.h"
#include "imageframe_time_axis_view.h"
#include "marker_time_axis_view.h"
#include "imageframe_view.h"
#include "marker_time_axis.h"
#include "marker_view.h"
#include "utils.h"
#include "prompter.h"
#include "rgb_macros.h"
#include "canvas_impl.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace sigc;
using namespace Gtk;
	
/**
 * Abstract Constructor for base visual time axis classes
 *
 * @param name the name/Id of thie TimeAxis
 * @param ed the Ardour PublicEditor
 * @param sess the current session
 * @param canvas the parent canvas object
 */
VisualTimeAxis::VisualTimeAxis(const string & name, PublicEditor& ed, ARDOUR::Session& sess, Canvas& canvas)
	: AxisView(sess),
	  TimeAxisView(sess,ed,(TimeAxisView*) 0, canvas),
	  visual_button (_("v")),
	  size_button (_("h"))
{
	time_axis_name = name ;
	_color = unique_random_color() ;
	_marked_for_display = true;
	
	name_entry.signal_activate().connect(mem_fun(*this, &VisualTimeAxis::name_entry_changed)) ;
	name_entry.signal_button_press_event().connect(mem_fun(*this, &VisualTimeAxis::name_entry_button_press_handler)) ;
	name_entry.signal_button_release_event().connect(mem_fun(*this, &VisualTimeAxis::name_entry_button_release_handler)) ;
	name_entry.signal_key_release_event().connect(mem_fun(*this, &VisualTimeAxis::name_entry_key_release_handler)) ;
	
	size_button.set_name("TrackSizeButton") ;
	visual_button.set_name("TrackVisualButton") ;
	hide_button.set_name("TrackRemoveButton") ;
	hide_button.add(*(Gtk::manage(new Gtk::Image(get_xpm("small_x.xpm")))));
	size_button.signal_button_release_event().connect (mem_fun (*this, &VisualTimeAxis::size_click)) ;
	visual_button.signal_clicked().connect (mem_fun (*this, &VisualTimeAxis::visual_click)) ;
	hide_button.signal_clicked().connect (mem_fun (*this, &VisualTimeAxis::hide_click)) ;
	ARDOUR_UI::instance()->tooltips().set_tip(size_button,_("Display Height")) ;
	ARDOUR_UI::instance()->tooltips().set_tip(visual_button, _("Visual options")) ;
	ARDOUR_UI::instance()->tooltips().set_tip(hide_button, _("Hide this track")) ;
		
	controls_table.attach (hide_button, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (visual_button, 1, 2, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (size_button, 2, 3, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	/* remove focus from the buttons */
	size_button.unset_flags(Gtk::CAN_FOCUS) ;
	hide_button.unset_flags(Gtk::CAN_FOCUS) ;
	visual_button.unset_flags(Gtk::CAN_FOCUS) ;
	
	set_height(Normal) ;
}

/**
 * VisualTimeAxis Destructor
 *
 */
VisualTimeAxis::~VisualTimeAxis()
{
}


//---------------------------------------------------------------------------------------//
// Name/Id Accessors/Mutators

void
VisualTimeAxis::set_time_axis_name(const string & name, void* src)
{
	std::string old_name = time_axis_name ;
	
	if(name != time_axis_name)
	{
		time_axis_name = name ;
		label_view() ;
		editor.route_name_changed(this) ;
	
		 NameChanged(time_axis_name, old_name, src) ; /* EMIT_SIGNAL */
	}
}

std::string
VisualTimeAxis::name() const
{
	return(time_axis_name) ;
}


//---------------------------------------------------------------------------------------//
// ui methods & data

/**
 * Sets the height of this TrackView to one of the defined TrackHeghts
 *
 * @param h the TrackHeight value to set
 */
void
VisualTimeAxis::set_height(TrackHeight h)
{
	TimeAxisView::set_height(h) ;
	
	switch (height)
	{
		case Largest:
		case Large:
		case Larger:
		case Normal:
		{
			hide_name_label ();
			show_name_entry ();
			other_button_hbox.show_all() ;
			break;
		}
		case Smaller:
		{
			hide_name_label ();
			show_name_entry ();
			other_button_hbox.hide_all() ;
			break;
		}
		case Small:
		{
			hide_name_entry ();
			show_name_label ();
			other_button_hbox.hide_all() ;
		}
		break;
	}
}

/**
 * Handle the visuals button click
 *
 */
void
VisualTimeAxis::visual_click()
{
	popup_display_menu(0);
}


/**
 * Handle the hide buttons click
 *
 */
void
VisualTimeAxis::hide_click()
{
	editor.hide_track_in_display (*this);
}


/**
 * Allows the selection of a new color for this TimeAxis
 *
 */
void
VisualTimeAxis::select_track_color ()
{
	if(choose_time_axis_color())
	{
		//Does nothing at this abstract point
	}
}

/**
 * Provides a color chooser for the selection of a new time axis color.
 *
 */
bool
VisualTimeAxis::choose_time_axis_color()
{
	bool picked ;
	Gdk::Color color ;
	gdouble current[4] ;
	Gdk::Color current_color ;
	
	current[0] = _color.get_red() / 65535.0 ;
	current[1] = _color.get_green() / 65535.0 ;
	current[2] = _color.get_blue() / 65535.0 ;
	current[3] = 1.0 ;

	current_color.set_rgb_p (current[0],current[1],current[2]);
	color = Gtkmm2ext::UI::instance()->get_color(_("ardour: color selection"),picked, &current_color) ;
	
	if (picked)
	{
		set_time_axis_color(color) ;
	}
	return(picked) ;
}

/**
 * Sets the color of this TimeAxis to the specified color c
 *
 * @param c the new TimeAxis color
 */
void
VisualTimeAxis::set_time_axis_color(Gdk::Color c)
{
	_color = c ;
}

void
VisualTimeAxis::set_selected_regionviews (AudioRegionSelection& regions)
{
	// Not handled by purely visual TimeAxis
}

//---------------------------------------------------------------------------------------//
// Handle time axis removal

/**
 * Handles the Removal of this VisualTimeAxis
 *
 * @param src the identity of the object that initiated the change
 */
void
VisualTimeAxis::remove_this_time_axis(void* src)
{
	vector<string> choices;

	std::string prompt  = string_compose (_("Do you really want to remove track \"%1\" ?\n(cannot be undone)"), time_axis_name);

	choices.push_back (_("Yes, remove it."));
	choices.push_back (_("No, do nothing."));

	Gtkmm2ext::Choice prompter (prompt, choices);

	if (prompter.run () == RESPONSE_ACCEPT) {
		if (prompter.get_choice() == 0) {
			/*
			  defer to idle loop, otherwise we'll delete this object
			  while we're still inside this function ...
			*/
			Glib::signal_idle().connect(bind(sigc::ptr_fun(&VisualTimeAxis::idle_remove_this_time_axis), this, src));
		}
	}
}

/**
 * Callback used to remove this time axis during the gtk idle loop
 * This is used to avoid deleting the obejct while inside the remove_this_time_axis
 * method
 *
 * @param ta the VisualTimeAxis to remove
 * @param src the identity of the object that initiated the change
 */
gint
VisualTimeAxis::idle_remove_this_time_axis(VisualTimeAxis* ta, void* src)
{
	 ta->VisualTimeAxisRemoved(ta->name(), src) ; /* EMIT_SIGNAL */
	delete ta ;
	ta = 0 ;
	return(false) ;
}




//---------------------------------------------------------------------------------------//
// Handle TimeAxis rename
		
/**
 * Construct a new prompt to receive a new name for this TimeAxis
 *
 * @see finish_time_axis_rename()
 */
void
VisualTimeAxis::start_time_axis_rename()
{
	ArdourPrompter name_prompter;

	name_prompter.set_prompt (_("new name: ")) ;
	name_prompter.show_all() ;

	switch (name_prompter.run ()) {
	case Gtk::RESPONSE_ACCEPT:
	  string result;
	  name_prompter.get_result (result);
	  if (result.length()) {
		  if (editor.get_named_time_axis(result) != 0) {
		    ARDOUR_UI::instance()->popup_error (_("A track already exists with that name"));
		    return ;
		  }
	  
		  set_time_axis_name(result, this) ;
	  }
	}
	label_view() ;
}

/**
 * Handles the new name for this TimeAxis from the name prompt
 *
 * @see start_time_axis_rename()
 */

void
VisualTimeAxis::label_view()
{
	name_label.set_text(time_axis_name) ;
	name_entry.set_text(time_axis_name) ;
	ARDOUR_UI::instance()->tooltips().set_tip(name_entry, time_axis_name) ;
}


//---------------------------------------------------------------------------------------//
// Handle name entry signals 

void
VisualTimeAxis::name_entry_changed()
{
	string x = name_entry.get_text ();
	
	if (x == time_axis_name) {
		return;
	}

	if (x.length() == 0) {
		name_entry.set_text (time_axis_name);
		return;
	}

	strip_whitespace_edges(x);

	if (!editor.get_named_time_axis(x)) {
		set_time_axis_name(x, this);
	} else {
		ARDOUR_UI::instance()->popup_error (_("a track already exists with that name"));
		name_entry.set_text(time_axis_name);
	}
}

gint 
VisualTimeAxis::name_entry_button_press_handler(GdkEventButton *ev)
{
	if (ev->button == 3) {
		return stop_signal (name_entry, "button_press_event");
	}
	return FALSE;
}

gint 
VisualTimeAxis::name_entry_button_release_handler(GdkEventButton *ev)
{
	return FALSE;
}

gint
VisualTimeAxis::name_entry_key_release_handler(GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Tab:
	case GDK_Up:
	case GDK_Down:
		name_entry_changed ();
		return TRUE;

	default:
		return FALSE;
	}
}


//---------------------------------------------------------------------------------------//
// Super class methods not handled by VisualTimeAxis
		
void
VisualTimeAxis::show_timestretch (jack_nframes_t start, jack_nframes_t end)
{
  // Not handled by purely visual TimeAxis
}

void
VisualTimeAxis::hide_timestretch()
{
  // Not handled by purely visual TimeAxis
}


