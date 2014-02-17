#include <string>

#include "pbd/file_utils.h"

std::string StatefulImage::_image_search_path;
StatefulImage::ImageCache StatefulImage::_image_cache;
PBD::Searchpath StatefulImage::_image_search_path;

StatefulImage::StatefulImage (const XMLNode& node)
	: _state (0)
	, font_description (0)
	, _text_x (0)
	, _text_y (0)
{
	if (load_states (node)) {
		throw failed_constructor();
	}
}

StatefulImage::~StatefulImage()
{
	delete font_description;
}

void
StatefulImage::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_states.empty() || _state >= _states.size()) {
		return;
	}
	ImageHandle image = _states[_state].image;
	Rect self = item_to_window (Rect (0, 0, image->get_width(), image->get_height()));

	boost::optional<Rect> draw = self.intersection (area);
	
	if (!draw) {
		return;
	}

	/* move the origin of the image to the right place on the surface
	   ("window" coordinates) and render it.
	*/
	context->set_source (image, self.x0, self.y0);
	context.rectangle (draw->x0, draw->y0, draw->width(), draw->height());
	context->fill ();

	if (_text) {
		Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

		layout->set_text (_text);

		if (_font_description) {
			layout->set_font_description (*_font_description);
		}

		// layout->set_alignment (_alignment);
		set_source_rgba (context, _text_color);
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
		const XMLProperty* prop;
		
		if ((prop = (*i)->property ("id")) == 0) {
			error << _("no ID for state") << endmsg;
			continue;
		}
		sscanf (prop->value().c_str(), "%ud", &s.id);

		if ((prop = (*i)->property ("image")) == 0) {
			error << _("no image for state") << endmsg;
			continue;
		}
		
		if ((s.image = find_image (prop->value())) == 0) {
			error << string_compose (_("image %1 not found for state"), prop->value()) << endmsg;
			continue;
		}
		
		if (_states.size() < s.id) {
			_states.reserve (s.id);
		}

		_states[s.id] = s;
	}


}

StatefulImage::ImageHandle
StatefulImage::find_image (const std::string& name)
{
	ImageCache::iterator i;

	if ((i = _image_cache.find (name)) != _image_cache.end()) {
		return *i;
	}

	std::string path;

	if (!find_file_in_search_path (_image_search_path, name, path)) {
		error << string_compose (_("Image named %1 not found"),
					 name) << endmsg;
		return ImageHandle();
	}
	
	return Cairo::Image::create_from_file (path);
}

void
StatefulImage::set_image_search_path (const std::string& path)
{
	_image_search_path = SearchPath (path);
}

void
StatefulImage::set_text (const std::string& text)
{
	_text = text;

	/* never alters bounding box */

	redraw ();
}

void
StatefulImage::set_state (States::size_type n)
{
	begin_change ();

	_state = n;
	_need_redraw = true;
	_bounding_box_dirty = true;

	end_change ();
}
