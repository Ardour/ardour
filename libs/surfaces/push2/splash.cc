/*
  Copyright (C) 2016 Paul Davis

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/i18n.h"
#include "pbd/search_path.h"

#include "ardour/filesystem_paths.h"

#include "splash.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace ArdourSurface;
using namespace ArdourCanvas;

SplashLayout::SplashLayout (Push2& p, Session& s)
	: Push2Layout (p, s)
{
	std::string splash_file;

	Searchpath rc (ARDOUR::ardour_data_search_path());
	rc.add_subdirectory_to_paths ("resources");

	if (!find_file (rc, PROGRAM_NAME "-splash.png", splash_file)) {
		cerr << "Cannot find splash screen image file\n";
		throw failed_constructor();
	}

	img = Cairo::ImageSurface::create_from_png (splash_file);
}

void
SplashLayout::render (Rect const& area, Cairo::RefPtr<Cairo::Context> context) const
{
	int rows = display_height ();
	int cols = display_width ();

	double x_ratio = (double) img->get_width() / (cols - 20);
	double y_ratio = (double) img->get_height() / (rows - 20);
	double scale = min (x_ratio, y_ratio);

	/* background */

	context->set_source_rgb (0.764, 0.882, 0.882);
	context->paint ();

	/* image */

	context->save ();
	context->translate (5, 5);
	context->scale (scale, scale);
	context->set_source (img, 0, 0);
	context->paint ();
	context->restore ();

	/* text */

	Glib::RefPtr<Pango::Layout> some_text = Pango::Layout::create (context);

	Pango::FontDescription fd ("Sans 38");
	some_text->set_font_description (fd);
	some_text->set_text (string_compose ("%1 %2", PROGRAM_NAME, VERSIONSTRING));

	context->move_to (200, 10);
	context->set_source_rgb (0, 0, 0);
	some_text->update_from_cairo_context (context);
	some_text->show_in_cairo_context (context);

	Pango::FontDescription fd2 ("Sans Italic 18");
	some_text->set_font_description (fd2);
	some_text->set_text (_("Ableton Push 2 Support"));

	context->move_to (200, 80);
	context->set_source_rgb (0, 0, 0);
	some_text->update_from_cairo_context (context);
	some_text->show_in_cairo_context (context);
}
