#ifndef __gtk2_ardour_ghost_event_h__
#define __gtk2_ardour_ghost_event_h__

#include <boost/unordered_map.hpp>

namespace ArdourCanvas {
	class Container;
	class Item;
}

class NoteBase;

class GhostEvent : public sigc::trackable
{
   public:
	GhostEvent (::NoteBase *, ArdourCanvas::Container *);
	GhostEvent (::NoteBase *, ArdourCanvas::Container *, ArdourCanvas::Item* i);
	virtual ~GhostEvent ();

	NoteBase* event;
	ArdourCanvas::Item* item;
	bool is_hit;
	int velocity_while_editing;

	/* must match typedef in NoteBase */
	typedef Evoral::Note<Temporal::Beats> NoteType;
	typedef boost::unordered_map<std::shared_ptr<NoteType>, GhostEvent* > EventList;

	static GhostEvent* find (std::shared_ptr<NoteType> parent, EventList& events, EventList::iterator& opti);
};


#endif /* __gtk2_ardour_ghost_event_h__ */
