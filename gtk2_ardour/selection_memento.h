#ifndef __ardour_gtk_selection_memento_h__
#define __ardour_gtk_selection_memento_h__

#include "pbd/statefuldestructible.h"

class Selection;

class SelectionMemento : public PBD::StatefulDestructible
{
public:
	SelectionMemento ();
	~SelectionMemento ();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);
};
#endif /* __ardour_gtk_selection_memento_h__ */
