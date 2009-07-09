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
	double          event_x;
	static double   middle_point, last_x;
	Gdk::Cursor     cursor;
	static NoteEnd  note_end;
	Editing::MidiEditMode edit_mode = _region.get_trackview()->editor().current_midi_edit_mode();

	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 2 ||
				(ev->button.button == 1 && edit_mode == Editing::MidiEditResize)) {
			double region_start = _region.get_position_pixels();
			event_x = ev->button.x;
			middle_point = region_start + x1() + (x2() - x1()) / 2.0L;

			if (event_x <= middle_point) {
				cursor = Gdk::Cursor(Gdk::LEFT_SIDE);
				note_end = NOTE_ON;
			} else {
				cursor = Gdk::Cursor(Gdk::RIGHT_SIDE);
				note_end = NOTE_OFF;
			}

			_item->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK, cursor, ev->motion.time);

			if (_region.mouse_state() == MidiRegionView::SelectTouchDragging) {
				_note_state = AbsoluteResize;
			} else {
				_note_state = RelativeResize;
			}

			_region.note_selected(this, true);
			_region.begin_resizing(note_end);
			last_x = event_x;

			return true;
		} 
		
	case GDK_MOTION_NOTIFY:
		event_x = ev->motion.x;

		if (_note_state == RelativeResize) {
			_region.update_resizing(note_end, event_x - last_x, true);
			last_x = event_x;
			return true;
		}

		if (_note_state == AbsoluteResize) {
			_region.update_resizing(note_end, event_x, false);
			return true;
		}

	case GDK_BUTTON_RELEASE:
		event_x = ev->button.x;

		switch (_note_state) {
		case RelativeResize: // Clicked
			_item->ungrab(ev->button.time);
			_region.commit_resizing(note_end, event_x, true);
			_note_state = None;
			return true;

		case AbsoluteResize: // Clicked
			_item->ungrab(ev->button.time);
			_region.commit_resizing(note_end, event_x, false);
			_note_state = None;
			return true;

		default:
			return CanvasNoteEvent::on_event(ev);
		}

	default:
		return CanvasNoteEvent::on_event(ev);
	}
}

} // namespace Gnome
} // namespace Canvas
