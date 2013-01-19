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

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/choice.h>

#include "ardour/session.h"
#include "ardour/utils.h"
#include "ardour/processor.h"
#include "ardour/location.h"

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
using namespace PBD;
using namespace Gtk;

/**
 * Abstract Constructor for base visual time axis classes
 *
 * @param name the name/Id of thie TimeAxis
 * @param ed the Ardour PublicEditor
 * @param sess the current session
 * @param canvas the parent canvas object
 */
VisualTimeAxis::VisualTimeAxis(const string & name, PublicEditor& ed, ARDOUR::Session* sess, Canvas& canvas)
	: AxisView(sess),
	  TimeAxisView(sess,ed,(TimeAxisView*) 0, canvas),
	  visual_button (_("v")),
	  size_button (_("h"))
{
	time_axis_name = name ;
	_color = unique_random_color() ;

	name_entry.signal_activate().connect(sigc::mem_fun(*this, &VisualTimeAxis::name_entry_changed)) ;
	name_entry.signal_button_press_event().connect(sigc::mem_fun(*this, &VisualTimeAxis::name_entry_button_press_handler)) ;
	name_entry.signal_button_release_event().connect(sigc::mem_fun(*this, &VisualTimeAxis::name_entry_button_release_handler)) ;
	name_entry.signal_key_release_event().connect(sigc::mem_fun(*this, &VisualTimeAxis::name_entry_key_release_handler)) ;

	size_button.set_name("TrackSizeButton") ;
	visual_button.set_name("TrackVisualButton") ;
	hide_button.set_name("TrackRemoveButton") ;
	hide_button.add(*(Gtk::manage(new Gtk::Image(get_xpm("small_x.xpm")))));
	size_button.signal_button_release_event().connect (sigc::mem_fun (*this, &VisualTimeAxis::size_click)) ;
	visual_button.signal_clicked().connect (sigc::mem_fun (*this, &VisualTimeAxis::visual_click)) ;
	hide_button.signal_clicked().connect (sigc::mem_fun (*this, &VisualTimeAxis::hide_click)) ;
	ARDOUR_UI::instance()->set_tip(size_button,_("Display Height")) ;
	ARDOUR_UI::instance()->set_tip(visual_button, _("Visual options")) ;
	ARDOUR_UI::instance()->set_tip(hide_button, _("Hide this track")) ;

	controls_table.attach (hide_button, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (visual_button, 1, 2, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (size_button, 2, 3, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	/* remove focus from the buttons */
	size_button.unset_flags(Gtk::CAN_FOCUS) ;
	hide_button.unset_flags(Gtk::CAN_FOCUS) ;
	visual_button.unset_flags(Gtk::CAN_FOCUS) ;

	set_height (hNormal) ;
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
 * @param h
 */
void
VisualTimeAxis::set_height(uint32_t h)
{
	TimeAxisView::set_height(h);

	if (h >= hNormal) {
		other_button_hbox.show_all() ;
	} else if (h >= hSmaller) {
		other_button_hbox.hide_all() ;
	} else if (h >= hSmall) {
		other_button_hbox.hide_all() ;
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
	// LAME fix for hide_button display refresh
	hide_button.set_sensitive(false);

	editor.hide_track_in_display (*this);

	hide_button.set_sensitive(true);
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
	color = Gtkmm2ext::UI::instance()->get_color(_("Color Selection"),picked, &current_color) ;

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
VisualTimeAxis::set_selected_regionviews (RegionSelection& regions)
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

	std::string prompt  = string_compose (_("Do you really want to remove track \"%1\" ?\n\nYou may also lose the playlist used by this track.\n\n(This action cannot be undone, and the session file will be overwritten)"), time_axis_name);

	choices.push_back (_("No, do nothing."));
	choices.push_back (_("Yes, remove it."));

	Gtkmm2ext::Choice prompter (prompt, choices);

	if (prompter.run () == 1) {
		/*
		  defer to idle loop, otherwise we'll delete this object
		  while we're still inside this function ...
		*/
		Glib::signal_idle().connect(sigc::bind(sigc::ptr_fun(&VisualTimeAxis::idle_remove_this_time_axis), this, src));
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
	name_prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	name_prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
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
	name_label.set_text (time_axis_name);
	name_entry.set_text (time_axis_name);
	ARDOUR_UI::instance()->set_tip (name_entry, Glib::Markup::escape_text (time_axis_name));
}


//---------------------------------------------------------------------------------------//
// Handle name entry signals

void
VisualTimeAxis::name_entry_changed()
{
	TimeAxisView::name_entry_changed ();

	string x = name_entry.get_text ();

	if (x == time_axis_name) {
		return;
	}

	strip_whitespace_edges(x);

	if (x.length() == 0) {
		name_entry.set_text (time_axis_name);
		return;
	}

	if (!editor.get_named_time_axis(x)) {
		set_time_axis_name (x, this);
	} else {
		ARDOUR_UI::instance()->popup_error (_("A track already exists with that name"));
		name_entry.set_text(time_axis_name);
	}
}

bool
VisualTimeAxis::name_entry_button_press_handler(GdkEventButton *ev)
{
	if (ev->button == 3) {
                return true;
	}
	return false
}

bool
VisualTimeAxis::name_entry_button_release_handler(GdkEventButton *ev)
{
	return false;
}

bool
VisualTimeAxis::name_entry_key_release_handler(GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Tab:
	case GDK_Up:
	case GDK_Down:
		name_entry_changed ();
		return true;

	default:
		break;
	}

        return false;
}


//---------------------------------------------------------------------------------------//
// Super class methods not handled by VisualTimeAxis

void
VisualTimeAxis::show_timestretch (framepos_t start, framepos_t end, int layers, int layer)
{
	// Not handled by purely visual TimeAxis
}

void
VisualTimeAxis::hide_timestretch()
{
	// Not handled by purely visual TimeAxis
}


