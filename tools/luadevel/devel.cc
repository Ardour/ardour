#include <stdint.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <list>
#include <vector>

#define LIBPBD_API
#include "../../libs/pbd/pbd/reallocpool.h"
#include "../../libs/pbd/reallocpool.cc"

#include <readline/readline.h>
#include <readline/history.h>

#include "lua/luastate.h"
#include "LuaBridge/LuaBridge.h"

static void my_lua_print (std::string s) {
	std::cout << s << "\n";
}


class A {
	public:
		A() { printf ("CTOR\n"); _int = 4; for (int i = 0; i < 256; ++i) {arr[i] = i; ar2[i] = i/256.0; ar3[i] = i;} }
		~A() { printf ("DTOR\n"); }

		void set_int (int a) { _int = a; }
		int  get_int () const { return _int; }

		int& get_ref () { return _int; }
		int get_arg (int &a) { printf ("a = %d\n", a);  a = _int; printf ("a = %d\n", a); return 1; }
		void get_arg2 (int &a, int& b) {  a = _int; b = 100; }
		void get_args (std::string &a) {  a = "hello"; }
		void set_ref (int const &a) { _int = a; }

		float * get_arr () { return arr; }
		float * get_ar2 () { return ar2; }
		int * get_ar3 () { return ar3; }

		void set_list (std::list<std::string> sl) { _sl = sl; }
		std::list<std::string>& get_list () { return _sl; }

		uint32_t minone() { return -1; }

		enum EN {
			RV1 = 1, RV2, RV3
		};

		enum EN ret_enum () { return _en;}
		void set_enum (enum EN en) { _en = en; }

	private:
		std::list<std::string> _sl;
		int _int;
		enum EN _en;
		float arr[256];
		float ar2[256];
		int ar3[256];
};

int main (int argc, char **argv)
{
#if 0
	LuaState lua;
#else
	PBD::ReallocPool _mempool ("Devel", 1048576);
	LuaState lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool));
#endif
	lua.Print.connect (&my_lua_print);
	lua_State* L = lua.getState();


#if 1
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.beginStdList <std::string> ("StringList")
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.beginStdVector <std::string> ("StringVector")
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.beginStdMap <std::string,std::string> ("StringStringMap")
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.beginStdSet <std::string> ("StringSet")
		.endClass ()
		.endNamespace ();


	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.registerArray <float> ("FloatArray")
		.registerArray <int> ("IntArray")
		.beginClass <A> ("A")
		.addConstructor <void (*) ()> ()
		.addFunction ("set_int", &A::set_int)
		.addFunction ("get_int", &A::get_int)
		.addRefFunction ("get_arg", &A::get_arg)
		.addRefFunction ("get_arg2", &A::get_arg2)
		.addRefFunction ("get_args", &A::get_args)
		.addFunction ("set_ref", &A::set_ref)
		.addFunction ("get_list", &A::get_list)
		.addFunction ("set_list", &A::set_list)
		.addFunction ("ret_enum", &A::ret_enum)
		.addFunction ("set_enum", &A::set_enum)
		.addFunction ("get_arr", &A::get_arr)
		.addFunction ("get_ar2", &A::get_ar2)
		.addFunction ("get_ar3", &A::get_ar3)
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.beginClass <A> ("A")
		.addFunction ("minone", &A::minone)
		.addConst ("cologne", 4711)
		.endClass ()
		.addConst ("koln", 4711)
		.endNamespace ();
#endif
#if 0 // session  script test
	lua.do_command (
			"function ArdourSession ()"
			"  local self = { scripts = {}, instances = {} }"
			""
			"  local foreach = function (fn)"
			"   for n, s in pairs (self.scripts) do"
			"    fn (n, s)"
			"   end"
			"  end"
			""
			"  local run = function ()"
			"   for n, s in pairs (self.instances) do"
			"     local status, err = pcall (s)"
			"     if not status then print ('fn \"'.. n .. '\": ', err) end"
			"   end"
			"   collectgarbage()"
			"  end"
			""
			"  local add = function (n, f, a)"
			"   assert(type(n) == 'string', 'function-name must be string')"
			"   assert(type(f) == 'function', 'Given script is a not a function')"
			"   assert(type(a) == 'table' or type(a) == 'nil', 'Given argument is invalid')"
			"   assert(self.scripts[n] == nil, 'Callback \"'.. n ..'\" already exists.')"
			"   self.scripts[n] = { ['f'] = f, ['a'] = a }"
			"   local env = { print = print, Session = Session, tostring = tostring, assert = assert, ipairs = ipairs, error = error, string = string, type = type, tonumber = tonumber, collectgarbage = collectgarbage, pairs = pairs, math = math, table = table, pcall = pcall }"
			"   self.instances[n] = load (string.dump(f), nil, nil, env)(a)"
			"  end"
			""
			"  local remove = function (n)"
			"   self.scripts[n] = nil"
			"  end"
			""
			"  local list = function ()"
			"   local rv = {}"
			"   foreach (function (n) rv[n] = true end)"
			"   return rv"
			"  end"
			""
			"  local function basic_serialize (o)"
			"    if type(o) == \"number\" then"
			"     return tostring(o)"
			"    else"
			"     return string.format(\"%q\", o)"
			"    end"
			"  end"
			""
			"  local function serialize (name, value)"
			"   local rv = name .. ' = '"
			"   if type(value) == \"number\" or type(value) == \"string\" or type(value) == \"nil\" then"
			"    return rv .. basic_serialize(value) .. ' '"
			"   elseif type(value) == \"table\" then"
			"    rv = rv .. '{} '"
			"    for k,v in pairs(value) do"
			"     local fieldname = string.format(\"%s[%s]\", name, basic_serialize(k))"
			"     rv = rv .. serialize(fieldname, v) .. ' '"
			"    end"
			"    return rv;"
			"   elseif type(value) == \"function\" then"
			"     return rv .. string.format(\"%q\", string.dump(value))"
			"   else"
			"    error('cannot save a ' .. type(value))"
			"   end"
			"  end"
			""
			""
			"  local save = function ()"
			"   return (serialize('scripts', self.scripts))"
			"  end"
			""
			"  local restore = function (state)"
			"   self.scripts = {}"
			"   load (state)()"
			"   print (scripts)"
			"   for n, s in pairs (scripts) do"
			"    add (n, load(s['f']), s['a'])"
			"   end"
			"  end"
			""
			" return { run = run, add = add, remove = remove,"
		  "          list = list, foreach = foreach,"
			"          restore = restore, save = save}"
			" end"
			" "
			" sess = ArdourSession ()"
			" ArdourSession = nil"
			);

	luabridge::LuaRef *lua_run;
	luabridge::LuaRef *lua_add;
	luabridge::LuaRef *lua_del;
	luabridge::LuaRef *lua_save;
	luabridge::LuaRef *lua_load;
	{
		luabridge::LuaRef lua_sess = luabridge::getGlobal (L, "sess");
		lua.do_command ("sess = nil"); // hide it.
		lua.do_command ("collectgarbage()");

		lua_run = new luabridge::LuaRef(lua_sess["run"]);
		lua_add = new luabridge::LuaRef(lua_sess["add"]);
		lua_del = new luabridge::LuaRef(lua_sess["remove"]);
		lua_save = new luabridge::LuaRef(lua_sess["save"]);
		lua_load = new luabridge::LuaRef(lua_sess["restore"]);
	}
	lua.do_command ("collectgarbage()");

#if 1
	lua.do_command ("function factory (t) return function () local p = t or { } local a = t[1] or 'Nibor' print ('Hello ' .. a) end end");
	luabridge::LuaRef lua_fact = luabridge::getGlobal (L, "factory");
	luabridge::LuaRef tbl_arg (luabridge::newTable(L));
	//tbl_arg[1] = "Robin";
	(*lua_add)("t2", lua_fact, tbl_arg);
#else
	lua.do_command ("function factory (t) return function () print ('Boo') end end");
	luabridge::LuaRef lua_fact = luabridge::getGlobal (L, "factory");
	(*lua_add)("t2", lua_fact());
#endif

	lua.do_command ("function factory (t) return function () print ('Ahoy') end end");
	luabridge::LuaRef lua_fact2 = luabridge::getGlobal (L, "factory");
	(*lua_add)("t1", lua_fact2);

	luabridge::LuaRef savedstate ((*lua_save)());
	std::string saved = savedstate.cast<std::string>();

	(*lua_del)("t2");

	try {
		(*lua_run)();
	} catch (luabridge::LuaException const& e) { printf ("LuaException: %s\n", e.what ()); }

	(*lua_load)(saved);

	for (int i = 0; i < 2; ++i) {
	lua.do_command ("collectgarbage()");
	lua.collect_garbage ();
	try {
		(*lua_run)();
	} catch (luabridge::LuaException const& e) { printf ("LuaException: %s\n", e.what ()); }
	}

#endif

	add_history("a = Test:A() b = 2 c = 3 d = 'a'");
	add_history("x = a:get_arg(b)  y = a:get_arg2(b, c)  z = a:get_args(d) ");
	add_history("for i,n in ipairs(y) do print (i, n); end");

	/////////////////////////////////////////////////////////////////////////////
	char *line;
	while ((line = readline ("> "))) {
		if (!strcmp (line, "quit")) {
			break;
		}
		if (strlen(line) == 0) {
			//lua.do_command("collectgarbage();");
			continue;
		}
		if (!lua.do_command (line)) {
			add_history(line); // OK
		} else {
			add_history(line); // :)
		}
	}
	printf("\n");
	return 0;
}
