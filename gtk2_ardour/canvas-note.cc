#include "canvas-note.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "evoral/Note.hpp"

using namespace ARDOUR;

namespace Gnome {
namespace Canvas {

CanvasNote::CanvasNote (MidiRegionView&                   region,
                        Group&                            group,
                        const boost::shared_ptr<NoteType> note,
                        bool with_events)
	: SimpleRect(group), CanvasNoteEvent(region, this, note)
{
	if (with_events) {
		signal_event().connect (sigc::mem_fun (*this, &CanvasNote::on_event));
	}
}

bool
CanvasNote::on_event(GdkEvent* ev)
{
	bool r = true;

	if (!CanvasNoteEvent::on_event (ev)) {
		r = _region.get_time_axis_view().editor().canvas_note_event (ev, this);
	}

	if (ev->type == GDK_BUTTON_RELEASE) {
		_region.note_button_release ();
	}
	
	return r;
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
