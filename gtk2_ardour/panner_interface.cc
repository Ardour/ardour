/*
    Copyright (C) 2011 Paul Davis

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

#include <gtkmm.h>
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/persistent_tooltip.h"

#include "pbd/file_utils.h"
#include "pbd/error.h"

#include "ardour/filesystem_paths.h"

#include "panner_interface.h"
#include "panner_editor.h"
#include "global_signals.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using namespace Gtkmm2ext;

const char* PannerInterface::_knob_image_files[101] = {
	"001.png", "002.png", "003.png", "004.png", "005.png", "006.png", "007.png", "008.png", "009.png", "010.png", 
	"011.png", "012.png", "013.png", "014.png", "015.png", "016.png", "017.png", "018.png", "019.png", "020.png",
	"021.png", "022.png", "023.png", "024.png", "025.png", "026.png", "027.png", "028.png", "029.png", "030.png",
	"031.png", "032.png", "033.png", "034.png", "035.png", "036.png", "037.png", "038.png", "039.png", "040.png",
	"041.png", "042.png", "043.png", "044.png", "045.png", "046.png", "047.png", "048.png", "049.png", "050.png",
	"051.png", "052.png", "053.png", "054.png", "055.png", "056.png", "057.png", "058.png", "059.png", "060.png",
	"061.png", "062.png", "063.png", "064.png", "065.png", "066.png", "067.png", "068.png", "069.png", "070.png",
	"071.png", "072.png", "073.png", "074.png", "075.png", "076.png", "077.png", "078.png", "079.png", "080.png",
	"081.png", "082.png", "083.png", "084.png", "085.png", "086.png", "087.png", "088.png", "089.png", "090.png",
	"091.png", "092.png", "093.png", "094.png", "095.png", "096.png", "097.png", "098.png", "099.png", "100.png",
	"101.png"
};

Glib::RefPtr<Gdk::Pixbuf> PannerInterface::_knob_image[101];

Glib::RefPtr<Gdk::Pixbuf> 
PannerInterface::load_pixbuf (const std::string& name)
{
	PBD::Searchpath spath(ARDOUR::ardour_data_search_path());

	spath.add_subdirectory_to_paths("icons/stereo_panner");

	std::string data_file_path;

	if (!PBD::find_file (spath, name, data_file_path)) {
		PBD::fatal << string_compose (_("cannot find icon image for %1 using %2"), name, spath.to_string()) << endmsg;
	}

	Glib::RefPtr<Gdk::Pixbuf> img;
	try {
		img = Gdk::Pixbuf::create_from_file (data_file_path);
	} catch (const Gdk::PixbufError &e) {
		cerr << "Caught PixbufError: " << e.what() << endl;
	} catch (...) {
		PBD::error << string_compose (_("Caught exception while loading icon named %1"), name) << endmsg;
	}

	return img;
}

PannerInterface::PannerInterface (boost::shared_ptr<Panner> p)
	: _panner (p)
	, _tooltip (this)
	, _editor (0)
{
    set_flags (Gtk::CAN_FOCUS);

    add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|
                Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|
                Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|
                Gdk::SCROLL_MASK|
                Gdk::POINTER_MOTION_MASK);
}

PannerInterface::~PannerInterface ()
{
	delete _editor;
}

bool
PannerInterface::on_enter_notify_event (GdkEventCrossing *)
{
	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
PannerInterface::on_leave_notify_event (GdkEventCrossing *)
{
	Keyboard::magic_widget_drop_focus ();
	return false;
}

bool
PannerInterface::on_key_release_event (GdkEventKey*)
{
	return false;
}

void
PannerInterface::value_change ()
{
	set_tooltip ();
	queue_draw ();
}

bool
PannerInterface::on_button_press_event (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::is_edit_event (ev)) {
		edit ();
		return true;
	}

	return false;
}

bool
PannerInterface::on_button_release_event (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::is_edit_event (ev)) {
		/* We edited on the press, so claim the release */
		return true;
	}

	return false;
}

void
PannerInterface::edit ()
{
	delete _editor;
	_editor = editor ();
	_editor->show ();
}

PannerPersistentTooltip::PannerPersistentTooltip (Gtk::Widget* w)
	: PersistentTooltip (w)
	, _dragging (false)
{

}

void
PannerPersistentTooltip::target_start_drag ()
{
	_dragging = true;
}

void
PannerPersistentTooltip::target_stop_drag ()
{
	_dragging = false;
}

bool
PannerPersistentTooltip::dragging () const
{
	return _dragging;
}
