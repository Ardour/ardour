#include "canvas/canvas.h"
#include "canvas/rectangle.h"

using namespace ArdourCanvas;

int main ()
{
	ImageCanvas* c = new ImageCanvas;
	Rectangle* r = new Rectangle (c->root ());
	r->set (Rect (0, 0, 256, 256));
	c->render_to_image (Rect (0, 0, 1024, 1024));
	c->write_to_png ("foo.png");
}
