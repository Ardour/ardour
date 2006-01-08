#ifndef __ardour_state_manager_h__
#define __ardour_state_manager_h__

#include <list>
#include <string>
#include <vector>

#include <sigc++/signal.h>

#include <ardour/ardour.h>

namespace ARDOUR {

typedef uint32_t state_id_t;


class StateManager : public sigc::trackable
{
  public:
	struct State {
	    std::string operation;
	    State (std::string why) : operation (why) {}
	    virtual ~State() {}
	};

	typedef std::list<State*> StateMap;

	StateManager ();
	virtual ~StateManager ();
	
	virtual void drop_all_states ();
	virtual void use_state (state_id_t);
	virtual void save_state (std::string why);

	sigc::signal<void,Change> StateChanged;

	state_id_t _current_state_id;

	static void set_allow_save (bool);
	static bool allow_save ();

  protected:
	static bool _allow_save;
	typedef std::pair<StateManager*,std::string> DeferredSave;
	static std::vector<DeferredSave> deferred;

	StateMap   states;

	virtual Change   restore_state (State&) = 0;
	virtual State* state_factory (std::string why) const = 0;
	virtual void   send_state_changed (Change);
};

} // namespace ARDOUR

#endif /* __ardour_state_manager_h__ */
