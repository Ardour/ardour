#include "canvas/root_group.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

RootGroup::RootGroup (Canvas* canvas)
	: Group (canvas)
{
#ifdef CANVAS_DEBUG
	name = "ROOT";
#endif	
}

void
RootGroup::compute_bounding_box () const
{
	Group::compute_bounding_box ();

	if (_bounding_box) {
		_canvas->request_size (Duple (_bounding_box.get().width (), _bounding_box.get().height ()));
	}
}
