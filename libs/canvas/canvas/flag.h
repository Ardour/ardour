#include "canvas/group.h"
#include "canvas/types.h"

namespace ArdourCanvas {

class Text;
class Line;
class Rectangle;

class Flag : public Group
{
public:
	Flag (Group *, Distance, Color, Color, Duple);

	void set_text (std::string const &);
	void set_height (Distance);
	
private:
	Distance _height;
	Color _outline_color;
	Color _fill_color;
	Text* _text;
	Line* _line;
	Rectangle* _rectangle;
};
	
}
