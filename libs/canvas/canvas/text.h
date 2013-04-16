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

	void set (std::string const &);
	void set_color (uint32_t);
	void set_font_description (Pango::FontDescription);
	void set_alignment (Pango::Alignment);

        void clamp_width (double);

        void set_size_chars (int nchars);
        void dump (std::ostream&) const;

private:
	std::string      _text;
	uint32_t         _color;
	Pango::FontDescription* _font_description;
	Pango::Alignment _alignment;
        mutable Cairo::RefPtr<Cairo::ImageSurface> _image;
        mutable Duple _origin;
        mutable double _width;
        mutable double _height;
        mutable bool _need_redraw;
        double _clamped_width;

        void redraw (Cairo::RefPtr<Cairo::Context>) const;
};

}

#endif /* __ardour_canvas_text_h__ */
