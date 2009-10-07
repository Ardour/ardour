#include "canvas-note.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "evoral/Note.hpp"
#include "utils.h"

using namespace ARDOUR;

namespace Gnome {
namespace Canvas {

bool
CanvasHit::on_event(GdkEvent* ev)
{
	if (!_region.get_trackview().editor().canvas_note_event (ev, this)) {
		return CanvasNoteEvent::on_event (ev);
	} else {
		return true;
	}
}

void 
CanvasHit::move_event(double dx, double dy)
{
	move (dx, dy);

#if 0
	cerr << "Move event by " << dx << " " << dy << endl;

	points->coords[0] += dx;
	points->coords[1] += dy;

	points->coords[2] += dx;
	points->coords[3] += dy;

	points->coords[4] += dx;
	points->coords[5] += dy;

	points->coords[6] += dx;
	points->coords[7] += dy;

	cerr << "Coords now " << endl
	     << '\t' << points->coords[0] << ", " << points->coords[1] << endl
	     << '\t' << points->coords[2] << ", " << points->coords[3] << endl
	     << '\t' << points->coords[4] << ", " << points->coords[5] << endl
	     << '\t' << points->coords[6] << ", " << points->coords[7] << endl
		;

	if (_text) {
		_text->property_x() = _text->property_x() + dx;
		_text->property_y() = _text->property_y() + dy;
	}

	hide ();
	show ();
	// request_update ();
#endif
}

} // namespace Gnome
} // namespace Canvas
