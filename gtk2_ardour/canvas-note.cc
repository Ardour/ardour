#include "canvas-note.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "evoral/Note.hpp"

using namespace ARDOUR;

namespace Gnome {
namespace Canvas {

bool
CanvasNote::on_event(GdkEvent* ev)
{
	if (!_region.get_trackview().editor().canvas_note_event (ev, this)) {
		return CanvasNoteEvent::on_event (ev);
	} else {
		return true;
	}
}

} // namespace Gnome
} // namespace Canvas
