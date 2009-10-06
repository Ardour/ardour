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

void 
CanvasNote::move_event(double dx, double dy)
{
	property_x1() = property_x1() + dx;
	property_y1() = property_y1() + dy;
	property_x2() = property_x2() + dx;
	property_y2() = property_y2() + dy;

	if (_text) {
		_text->hide();
		_text->property_x() = _text->property_x() + dx;
		_text->property_y() = _text->property_y() + dy;
		_text->show();
	}
}


} // namespace Gnome
} // namespace Canvas
