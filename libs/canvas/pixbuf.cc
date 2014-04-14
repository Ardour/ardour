/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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
//#include "gtkmm/messagedialog.h"

#include <cairomm/cairomm.h>
#include <gdkmm/general.h>

#include "canvas/pixbuf.h"
#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/search_path.h"
#include "pbd/file_utils.h"
#include "ardour/filesystem_paths.h"

using namespace std;
using namespace ArdourCanvas;
using namespace PBD;

//#define dbg_msg(a) Gtk::MessageDialog (a, "ArdourCanvas::Pixbuf").run();

Pixbuf::Pixbuf (Group* g)
	: Item (g)
{
	
}

Pixbuf::Pixbuf (Group* g, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Item*>& named_items)
	: Item (g, definition, styles, named_items)
{
	std::string imagename = xml_property(definition, "file", styles, "");
	if (!imagename.empty())
	{
		Searchpath spath(ARDOUR::ardour_data_search_path());

		std::string dirname = Glib::path_get_dirname (imagename);
		std::string filename = Glib::path_get_basename (imagename);
		if (!dirname.empty()) {
			spath.add_subdirectory_to_paths(dirname);
		}

		std::string data_file_path;

		if (find_file_in_search_path (spath, filename, data_file_path)) {
			try {
				_pixbuf = Gdk::Pixbuf::create_from_file (data_file_path);
			} catch (const Gdk::PixbufError &e) {
				cerr << "Caught PixbufError: " << e.what() << endl;
			} catch (...) {
				cerr << string_compose ("Caught exception while loading icon named %1", imagename) << endmsg;
			}
		}
	}
}

void
Pixbuf::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rect self = item_to_window (Rect (position(), position().translate (Duple (_pixbuf->get_width(), _pixbuf->get_height()))));
	boost::optional<Rect> r = self.intersection (area);

	if (!r) {
		return;
	}

	Rect draw = r.get ();

	context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
	Gdk::Cairo::set_source_pixbuf (context, _pixbuf, draw.x0, draw.y0);
	context->paint ();
}
	
void
Pixbuf::compute_bounding_box () const
{
	if (_pixbuf) {
		_bounding_box = boost::optional<Rect> (Rect (0, 0, _pixbuf->get_width(), _pixbuf->get_height()));
	} else {
		_bounding_box = boost::optional<Rect> ();
	}

	_bounding_box_dirty = false;
}

void
Pixbuf::set (Glib::RefPtr<Gdk::Pixbuf> pixbuf)
{
	begin_change ();
	
	_pixbuf = pixbuf;
	_bounding_box_dirty = true;

	end_change ();
}

Glib::RefPtr<Gdk::Pixbuf>
Pixbuf::pixbuf() {
	return _pixbuf;
}

