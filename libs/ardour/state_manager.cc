#include <pbd/error.h>
#include <ardour/state_manager.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

bool StateManager::_allow_save = true;
sigc::signal<void,const char*> StateManager::SaveAllowed;

StateManager::StateManager ()
{
	_current_state_id = 0;
}

StateManager::~StateManager()
{
}

void
StateManager::prohibit_save ()
{
	_allow_save = false;
}

void
StateManager::allow_save (const char* why, bool do_save)
{
	_allow_save = true;
	if (do_save) {
		SaveAllowed (why);
		SaveAllowed.slots().erase (SaveAllowed.slots().begin(), SaveAllowed.slots().end());
	}
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
		fatal << string_compose (_("programming error: illegal state ID (%1) passed to "
				    "StateManager::set_state() (range = 0-%2)"), id, states.size()-1)
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
	if (!_allow_save) {
		SaveAllowed.connect (mem_fun (*this, &StateManager::save_state));
		return;
	}

	states.push_back (state_factory (why));
	_current_state_id = states.size() - 1;
}

void
StateManager::send_state_changed (Change what_changed)
{
	StateChanged (what_changed);
}
