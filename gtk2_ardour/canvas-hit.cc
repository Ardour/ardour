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
	move_by (dx, dy);
}

} // namespace Gnome
} // namespace Canvas
