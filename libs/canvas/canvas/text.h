#ifndef __ardour_canvas_text_h__
#define __ardour_canvas_text_h__

#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>

#include "canvas/item.h"

namespace ArdourCanvas {

class Text : public Item
{
public:
	Text (Group *);
       ~Text();

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;
	XMLNode* get_state () const;
	void set_state (XMLNode const *);

	void set (std::string const &);
	void set_color (uint32_t);
	void set_font_description (Pango::FontDescription);
	void set_alignment (Pango::Alignment);

        void set_size_chars (int nchars);

private:
	std::string      _text;
        Cairo::RefPtr<Cairo::ImageSurface> _image;
	uint32_t         _color;
	Pango::FontDescription* _font_description;
	Pango::Alignment _alignment;
        Duple _origin;
        int _width;
        int _height;

        void redraw ();
};

}

#endif /* __ardour_canvas_text_h__ */
