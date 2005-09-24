#include <pbd/error.h>
#include <ardour/state_manager.h>

#include "i18n.h"

using namespace ARDOUR;

StateManager::StateManager ()
{
	_current_state_id = 0;
}

StateManager::~StateManager()
{
}

void
StateManager::drop_all_states ()
{
	for (StateMap::iterator i = states.begin(); i != states.end(); ++i) {
		delete *i;
	}

	states.clear ();

	save_state (_("cleared history"));
}

void
StateManager::use_state (state_id_t id)
{
	Change what_changed;
	state_id_t n;
	StateMap::iterator i;
	
	for (n = 0, i = states.begin(); n < id && i != states.end(); ++n, ++i);

	if (n != id || i == states.end()) {
		fatal << compose (_("programming error: illegal state ID (%1) passed to "
				    "StateManager::set_state() (range = 0-%3)"), id,
				  states.size()-1)
		      << endmsg;
		/*NOTREACHED*/
		return;
	}

	what_changed = restore_state (**i);
	_current_state_id = id;
	send_state_changed (what_changed);
}

void
StateManager::save_state (std::string why)
{
	if (!should_save_state()) 
		return;

	states.push_back (state_factory (why));
	_current_state_id = states.size() - 1;
}

void
StateManager::send_state_changed (Change what_changed)
{
	StateChanged (what_changed);
}
