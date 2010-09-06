#include "canvas-note.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "evoral/Note.hpp"
#include "utils.h"

using namespace ARDOUR;

namespace Gnome {
namespace Canvas {

CanvasHit::CanvasHit (MidiRegionView&                   region,
                      Group&                            group,
                      double                            size,
                      const boost::shared_ptr<NoteType> note,
                      bool with_events) 
        : Diamond(group, size)
        , CanvasNoteEvent(region, this, note)
{
        if (with_events) {
                signal_event().connect (sigc::mem_fun (*this, &CanvasHit::on_event));
        }
}

bool
CanvasHit::on_event(GdkEvent* ev)
{
        if (!CanvasNoteEvent::on_event (ev)) {
                return _region.get_time_axis_view().editor().canvas_note_event (ev, this);
	} 
        return true;
}

void
CanvasHit::move_event(double dx, double dy)
{
	move_by (dx, dy);
}

} // namespace Gnome
} // namespace Canvas
