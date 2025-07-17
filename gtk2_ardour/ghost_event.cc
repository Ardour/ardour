#include "canvas/container.h"
#include "canvas/debug.h"
#include "canvas/polygon.h"

#include "ghost_event.h"
#include "hit.h"
#include "note.h"
#include "note_base.h"

GhostEvent::GhostEvent (NoteBase* e, ArdourCanvas::Container* g, ArdourCanvas::Item* i)
	: event (e)
	, item (i)
	, is_hit (false)
{
	velocity_while_editing = event->note()->velocity();
	if (dynamic_cast<Hit*>(e)) {
		is_hit = true;
	}
}

GhostEvent::GhostEvent (NoteBase* e, ArdourCanvas::Container* g)
	: event (e)
{
	if (dynamic_cast<Note*>(e)) {
		item = new ArdourCanvas::Rectangle (g, ArdourCanvas::Rect(e->x0(), e->y0(), e->x1(), e->y1()));
		is_hit = false;
	} else {
		Hit* hit = dynamic_cast<Hit*>(e);
		if (!hit) {
			return;
		}
		ArdourCanvas::Polygon* poly = new ArdourCanvas::Polygon(g);
		poly->set(Hit::points(e->y1() - e->y0()));
		poly->set_position(hit->position());
		item = poly;
		is_hit = true;
	}

	velocity_while_editing = event->note()->velocity();

	CANVAS_DEBUG_NAME (item, "ghost note item");
}

GhostEvent::~GhostEvent ()
{
	/* event is not ours to delete */
	delete item;
}

void
GhostEvent::set_sensitive (bool yn)
{
	item->set_ignore_events (!yn);
}

/** Given a note in our parent region (ie the actual MidiRegionView), find our
 *  representation of it.
 *  @return Our Event, or 0 if not found.
 */
GhostEvent *
GhostEvent::find (std::shared_ptr<GhostEvent::NoteType> parent, EventList& events, EventList::iterator& opti)
{
	/* we are using _optimization_iterator to speed up the common case where a caller
	   is going through our notes in order.
	*/

	if (opti != events.end()) {
		++opti;
		if (opti != events.end() && opti->first == parent) {
			return opti->second;
		}
	}

	opti = events.find (parent);
	if (opti != events.end()) {
		return opti->second;
	}

	return nullptr;
}


