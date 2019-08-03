/*
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string>

#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>

#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/xml++.h"

#include "canvas/stateful_image.h"

#include "pbd/i18n.h"

using namespace ArdourCanvas;
using PBD::error;

PBD::Searchpath StatefulImage::_image_search_path;
StatefulImage::ImageCache StatefulImage::_image_cache;

StatefulImage::StatefulImage (Canvas* c, const XMLNode& node)
	: Item (c)
	, _state (0)
	, _font (0)
	, _text_x (0)
	, _text_y (0)
{
	if (load_states (node)) {
		throw failed_constructor();
	}
}

StatefulImage::~StatefulImage()
{
	delete _font;
}

void
StatefulImage::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_states.empty() || _state >= _states.size()) {
		return;
	}
	ImageHandle image = _states[_state].image;
	Rect self = item_to_window (Rect (0, 0, image->get_width(), image->get_height()));

	Rect draw = self.intersection (area);

	if (!draw) {
		return;
	}

	/* move the origin of the image to the right place on the surface
	   ("window" coordinates) and render it.
	*/
	context->set_source (image, self.x0, self.y0);
	context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
	context->fill ();

	if (!_text.empty()) {
		Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

		layout->set_text (_text);

		if (_font) {
			layout->set_font_description (*_font);
		}

		// layout->set_alignment (_alignment);
		Gtkmm2ext::set_source_rgba (context, _text_color);
		context->move_to (_text_x, _text_y);
		layout->show_in_cairo_context (context);
	}
}

void
StatefulImage::compute_bounding_box () const
{
	if (!_states.empty()) {

		/* all images are assumed to be the same size */

		_bounding_box = Rect (0, 0, _states[0].image->get_width(), _states[0].image->get_height());
	}
}

int
StatefulImage::load_states (const XMLNode& node)
{
	const XMLNodeList& nodes (node.children());

	_states.clear ();

	for (XMLNodeList::const_iterator i = nodes.begin(); i != nodes.end(); ++i) {
		State s;
		States::size_type id;
		const XMLProperty* prop;

		if ((prop = (*i)->property ("id")) == 0) {
			error << _("no ID for state") << endmsg;
			continue;
		}
		sscanf (prop->value().c_str(), "%" G_GSIZE_FORMAT, &id);

		if ((prop = (*i)->property ("image")) == 0) {
			error << _("no image for state") << endmsg;
			continue;
		}

		if (!(s.image = find_image (prop->value()))) {
			error << string_compose (_("image %1 not found for state"), prop->value()) << endmsg;
			continue;
		}

		if (_states.size() < id) {
			_states.reserve (id);
		}

		_states[id] = s;
	}

	return 0;
}

StatefulImage::ImageHandle
StatefulImage::find_image (const std::string& name)
{
	ImageCache::iterator i;

	if ((i = _image_cache.find (name)) != _image_cache.end()) {
		return i->second;
	}

	std::string path;

	if (!find_file (_image_search_path, name, path)) {
		error << string_compose (_("Image named %1 not found"),
					 name) << endmsg;
		return ImageHandle();
	}

	return Cairo::ImageSurface::create_from_png (path);
}

void
StatefulImage::set_image_search_path (const std::string& path)
{
	_image_search_path = PBD::Searchpath (path);
}

void
StatefulImage::set_text (const std::string& text)
{
	_text = text;

	/* never alters bounding box */

	redraw ();
}

bool
StatefulImage::set_state (States::size_type n)
{
	if (n >= _states.size()) {
		return false;
	}

	_state = n;
	redraw ();

	return true;
}
