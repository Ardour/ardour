#ifndef __gtkardour_luainstance_h__
#define __gtkardour_luainstance_h__

#include <bitset>

#include "pbd/id.h"
#include "pbd/signals.h"
#include "pbd/xml++.h"

#include "ardour/luascripting.h"
#include "ardour/lua_script_params.h"
#include "ardour/luabindings.h"
#include "ardour/session_handle.h"

#include "lua/luastate.h"

#include "luasignal.h"

namespace luabridge {
	class LuaRef;
}

typedef std::bitset<LuaSignal::LAST_SIGNAL> ActionHook;

class LuaCallback : public ARDOUR::SessionHandlePtr, public sigc::trackable
{
public:
	LuaCallback (ARDOUR::Session*, const std::string&, const std::string&, const ActionHook&, const ARDOUR::LuaScriptParamList&);
	LuaCallback (ARDOUR::Session*, XMLNode & node);
	~LuaCallback ();

	XMLNode& get_state (void);
	void set_session (ARDOUR::Session *);

	const PBD::ID& id () const { return _id; }
	const std::string& name () const { return _name; }
	ActionHook signals () const { return _signals; }
	bool lua_slot (std::string&, std::string&, ActionHook&, ARDOUR::LuaScriptParamList&);
	PBD::Signal0<void> drop_callback;

protected:
	void session_going_away ();

private:
	LuaState lua;

	PBD::ID _id;
	std::string _name;
	ActionHook _signals;

	void reconnect ();
	template <class T> void reconnect_object (T);
	void init ();

	luabridge::LuaRef * _lua_add;
	luabridge::LuaRef * _lua_get;
	luabridge::LuaRef * _lua_call;
	luabridge::LuaRef * _lua_save;
	luabridge::LuaRef * _lua_load;

	PBD::ScopedConnectionList _connections;

	template <typename T, typename S> void connect_0 (enum LuaSignal::LuaSignal, T, S*);
	template <typename T> void proxy_0 (enum LuaSignal::LuaSignal, T);

	template <typename T, typename C1> void connect_1 (enum LuaSignal::LuaSignal, T, PBD::Signal1<void, C1>*);
	template <typename T, typename C1> void proxy_1 (enum LuaSignal::LuaSignal, T, C1);

	template <typename T, typename C1, typename C2> void connect_2 (enum LuaSignal::LuaSignal, T, PBD::Signal2<void, C1, C2>*);
	template <typename T, typename C1, typename C2> void proxy_2 (enum LuaSignal::LuaSignal, T, C1, C2);
};

typedef boost::shared_ptr<LuaCallback> LuaCallbackPtr;
typedef std::map<PBD::ID, LuaCallbackPtr> LuaCallbackMap;



class LuaInstance : public PBD::ScopedConnectionList, public ARDOUR::SessionHandlePtr
{
public:
	static LuaInstance* instance();
	static void destroy_instance();
	~LuaInstance();

	static void register_classes (lua_State* L);
	static void register_hooks (lua_State* L);
	static void bind_cairo (lua_State* L);

	void set_session (ARDOUR::Session* s);

	int set_state (const XMLNode&);
	XMLNode& get_action_state (void);
	XMLNode& get_hook_state (void);

	bool interactive_add (ARDOUR::LuaScriptInfo::ScriptType, int);

	/* actions */
	void call_action (const int);

	bool set_lua_action (const int, const std::string&, const std::string&, const ARDOUR::LuaScriptParamList&);
	bool remove_lua_action (const int);
	bool lua_action_name (const int, std::string&);
	std::vector<std::string> lua_action_names ();
	bool lua_action (const int, std::string&, std::string&, ARDOUR::LuaScriptParamList&);
	sigc::signal<void,int,std::string> ActionChanged;

	/* callbacks */
	bool register_lua_slot (const std::string&, const std::string&, const ARDOUR::LuaScriptParamList&);
	bool unregister_lua_slot (const PBD::ID&);
	std::vector<PBD::ID> lua_slots () const;
	bool lua_slot_name (const PBD::ID&, std::string&) const;
	std::vector<std::string> lua_slot_names () const;
	bool lua_slot (const PBD::ID&, std::string&, std::string&, ActionHook&, ARDOUR::LuaScriptParamList&);
	sigc::signal<void,PBD::ID,std::string,ActionHook> SlotChanged;

	static PBD::Signal0<void> LuaTimerDS; // deci-seconds (Timer every .1s)

private:
	LuaInstance();
	static LuaInstance* _instance;

	void init ();
	void session_going_away ();

	LuaState lua;

	luabridge::LuaRef * _lua_call_action;
	luabridge::LuaRef * _lua_add_action;
	luabridge::LuaRef * _lua_del_action;
	luabridge::LuaRef * _lua_get_action;

	luabridge::LuaRef * _lua_load;
	luabridge::LuaRef * _lua_save;
	luabridge::LuaRef * _lua_clear;

	LuaCallbackMap _callbacks;
	PBD::ScopedConnectionList _slotcon;

	void every_point_one_seconds ();
	sigc::connection point_one_second_connection;
};

#endif
