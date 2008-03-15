#include "canvas-note.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "ardour/note.h"

using namespace ARDOUR;

namespace Gnome {
namespace Canvas {

bool
CanvasNote::on_event(GdkEvent* ev)
{
	double          event_x;
	static double   middle_point, pressed_x, last_x;
	Gdk::Cursor     cursor;
	static NoteEnd  note_end;

	switch(ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 2) {
			event_x = ev->button.x;
			middle_point = x1() + (x2() - x1()) / 2.0L;

			if(event_x <= middle_point) {
				cursor = Gdk::Cursor(Gdk::LEFT_SIDE);
				last_x = x1();
				note_end = NOTE_ON;
			} else {
				cursor = Gdk::Cursor(Gdk::RIGHT_SIDE);
				last_x = x2();
				note_end = NOTE_OFF;
			}

			_item->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK, cursor, ev->motion.time);

			if (_region.mouse_state() == MidiRegionView::SelectTouchDragging) {
				_mouse2_state = AbsoluteResize;
			} else {
				_mouse2_state = RelativeResize;
			}

			pressed_x = event_x;

			_region.note_selected(this, true);
			_region.begin_resizing(note_end);

			return true;
		}

	case GDK_MOTION_NOTIFY:
		event_x = ev->motion.x;

		if (_mouse2_state == RelativeResize) {
			_region.update_resizing(note_end, event_x - last_x, true);
			last_x = event_x;
			return true;
		}

		if (_mouse2_state == AbsoluteResize) {
			_region.update_resizing(note_end, event_x, false);
			return true;
		}

	case GDK_BUTTON_RELEASE:
		event_x = ev->button.x;

		switch (_mouse2_state) {
		case RelativeResize: // Clicked
			_item->ungrab(ev->button.time);
			_region.commit_resizing(note_end, event_x, true);
			_mouse2_state = None;
			return true;

		case AbsoluteResize: // Clicked
			_item->ungrab(ev->button.time);
			_region.commit_resizing(note_end, event_x, false);
			_mouse2_state = None;
			return true;

		default:
			return CanvasMidiEvent::on_event(ev);
		}

	default:
		return CanvasMidiEvent::on_event(ev);
	}
}

} // namespace Gnome
} // namespace Canvas
