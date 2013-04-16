#include "canvas/item.h"

namespace ArdourCanvas {

class LineSet : public Item
{
public:
	enum Orientation {
		Vertical,
		Horizontal
	};

	LineSet (Group *);

	void compute_bounding_box () const;
	void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;

	void set_height (Distance);

	void add (Coord, Distance, Color);
	void clear ();

	struct Line {
		Line (Coord y_, Distance width_, Color color_) : y (y_), width (width_), color (color_) {}
		
		Coord y;
		Distance width;
		Color color;
	};

private:
	std::list<Line> _lines;
	Distance _height;
};

}
