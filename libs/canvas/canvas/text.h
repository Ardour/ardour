#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>

#include "canvas/item.h"

namespace ArdourCanvas {

class Text : public Item
{
public:
	Text (Group *);

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;
	XMLNode* get_state () const;
	void set_state (XMLNode const *);

	void set (std::string const &);
	void set_color (uint32_t);
	void set_font_description (Pango::FontDescription *);
	void set_alignment (Pango::Alignment);

private:
	Glib::RefPtr<Pango::Layout> layout (Cairo::RefPtr<Cairo::Context>) const;
	
	std::string _text;
	Pango::FontDescription* _font_description;
	uint32_t _color;
	Pango::Alignment _alignment;
};

}
