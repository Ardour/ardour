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

luabridge::LuaRef::Proxy&
luabridge::LuaRef::Proxy::clone_instance (const void* classkey, void* p) {
  lua_rawgeti (m_L, LUA_REGISTRYINDEX, m_tableRef);
  lua_rawgeti (m_L, LUA_REGISTRYINDEX, m_keyRef);

	luabridge::UserdataPtr::push_raw (m_L, p, classkey);

  lua_rawset (m_L, -3);
  lua_pop (m_L, 1);
  return *this;
}



class A {
	public:
		A() { printf ("CTOR %p\n", this); _int = 4; for (int i = 0; i < 256; ++i) {arr[i] = i; ar2[i] = i/256.0; ar3[i] = i;} }
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
		void pointer (float*f) const { printf ("PTR %p", f); }

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


class LuaTableRef {
	public:
		LuaTableRef () {}
		~LuaTableRef () {}

		int get (lua_State* L) {
			luabridge::LuaRef rv (luabridge::newTable (L));
			for (std::vector<LuaTableEntry>::const_iterator i = _data.begin (); i != _data.end (); ++i) {
				switch ((*i).keytype) {
					case LUA_TSTRING:
						assign(&rv, i->k_s, *i);
						break;
					case LUA_TNUMBER:
						assign(&rv, i->k_n, *i);
						break;
				}
			}
			luabridge::push (L, rv);
			return 1;
		}

		int set (lua_State* L) {
			if (!lua_istable (L, -1)) { return luaL_error (L, "argument is not a table"); }
			_data.clear ();

			lua_pushvalue (L, -1);
			lua_pushnil (L);
			while (lua_next (L, -2)) {
				lua_pushvalue (L, -2);

				LuaTableEntry s (lua_type(L, -1), lua_type(L, -2));
				switch (lua_type(L, -1)) {
					case LUA_TSTRING:
						s.k_s = luabridge::Stack<std::string>::get (L, -1);
						break;
						;
					case LUA_TNUMBER:
						s.k_n = luabridge::Stack<unsigned int>::get (L, -1);
						break;
					default:
						// invalid key
						lua_pop (L, 2);
						continue;
				}

				switch(lua_type(L, -2)) {
					case LUA_TSTRING:
						s.s = luabridge::Stack<std::string>::get (L, -2);
						break;
					case LUA_TBOOLEAN:
						s.b = lua_toboolean (L, -2);
						break;
					case LUA_TNUMBER:
						s.n = lua_tonumber (L, -2);
						break;
					case LUA_TUSERDATA:
						{
							bool ok = false;
							lua_getmetatable (L, -2);
							lua_rawgetp (L, -1, luabridge::getIdentityKey ());
							if (lua_isboolean (L, -1)) {
								lua_pop (L, 1);
								const void* key = lua_topointer (L, -1);
								lua_pop (L, 1);
								void const* classkey = findclasskey (L, key);

								if (classkey) {
									ok = true;
									s.c = classkey;
									s.p = luabridge::Userdata::get_ptr (L, -2);
								}
							}  else {
								lua_pop (L, 2);
							}

							if (ok) {
								break;
							}
							// invalid userdata -- fall through
						}
						// no break
					case LUA_TFUNCTION: // no support -- we could... string.format("%q", string.dump(value, true))
					case LUA_TTABLE: // no nested tables, sorry.
					case LUA_TNIL: // fallthrough
					default:
						// invalid value
						lua_pop (L, 2);
						continue;
				}

				_data.push_back(s);
				lua_pop (L, 2);
			}
			return 0;
		}

		static void* findclasskey (lua_State *L, const void* key)
		{
			lua_pushvalue(L, LUA_REGISTRYINDEX);
			lua_pushnil (L);
			while (lua_next (L, -2)) {
				lua_pushvalue (L, -2);
				if (lua_topointer(L, -2) == key) {
					void* rv = lua_touserdata (L, -1);
					lua_pop (L, 4);
					return rv;
				}
				lua_pop (L, 2);
			}
			lua_pop (L, 1);
			return NULL;
		}

	private:
		struct LuaTableEntry {
			LuaTableEntry (int kt, int vt)
				: keytype (kt)
				, valuetype (vt)
			{ }

			int keytype;
			std::string k_s;
			unsigned int k_n;

			int valuetype;
			// LUA_TUSERDATA
			const void* c;
			void* p;
			// LUA_TBOOLEAN
			bool b;
			// LUA_TSTRING:
			std::string s;
			// LUA_TNUMBER:
			double n;
		};

		template<typename T>
		static void assign (luabridge::LuaRef* rv, T key, const LuaTableEntry& s)
		{
			switch (s.valuetype) {
				case LUA_TSTRING:
					(*rv)[key] = s.s;
					break;
				case LUA_TBOOLEAN:
					(*rv)[key] = s.b;
					break;
				case LUA_TNUMBER:
					(*rv)[key] = s.n;
					break;
				case LUA_TUSERDATA:
					(*rv)[key].clone_instance (s.c, s.p);
					break;
				default:
					assert (0);
					break;
			}
		}

		std::vector<LuaTableEntry> _data;
};


#if 0
static void* findclasskey (lua_State *L, const void* key)
{
	lua_pushvalue(L, LUA_REGISTRYINDEX);
	lua_pushnil (L);
	while (lua_next (L, -2)) {
		lua_pushvalue (L, -2);
		if (lua_topointer(L, -2) == key) {
			void* rv = lua_touserdata (L, -1);
			lua_pop (L, 4);
			return rv;
		}
		lua_pop (L, 2);
	}
	lua_pop (L, 1);
	return NULL;
}

static int tableSerialize (lua_State *L)
{
	if (!lua_istable (L, -1)) { return luaL_error (L, "argument is not a table"); }

	luabridge::LuaRef rv (luabridge::newTable (L));
	std::cout << "CLASS A KEY: " << luabridge::ClassInfo <A>::getClassKey () << "\n";
	lua_rawgetp (L, LUA_REGISTRYINDEX, luabridge::ClassInfo <A>::getClassKey ());
	std::cout << " CLASS A TABLE PTR=" << lua_topointer (L, -1) << "\n";
	lua_pop (L, 1);
	// for k,v in pairs (debug.getregistry ()) do print (k,v) end

	lua_pushvalue (L, -1);
	lua_pushnil (L);
	while (lua_next (L, -2)) {
		lua_pushvalue (L, -2);
		unsigned int const i = luabridge::Stack<unsigned int>::get (L, -1);
		int t = lua_type(L, -2);
		switch(t) {
			case LUA_TSTRING:
				std::cout << "  " << i << ": '" << lua_tostring(L, -2) << "'\n";
				rv[i] = lua_tostring(L, -2);
				break;
			case LUA_TBOOLEAN:
				std::cout << "  " << i << ": " <<
					(lua_toboolean(L, -2) ? "true" : "false") << "\n";
				rv[i] = lua_toboolean(L, -2);
				break;
			case LUA_TNUMBER:
				std::cout << "  " << i << ": " << lua_tonumber(L, -2) << "\n";
				rv[i] = lua_tonumber(L, -2);
				break;
			case LUA_TUSERDATA:
				{
					lua_getmetatable (L, -2);
					lua_rawgetp (L, -1, luabridge::getIdentityKey ());
					if (lua_isboolean (L, -1)) {
						lua_pop (L, 1);
						const void* key = lua_topointer (L, -1);
						lua_pop (L, 1);
						void const* classkey = findclasskey (L, key);

						if (classkey) {
							void* p = luabridge::Userdata::get_ptr (L, -2);
							rv[i].clone_instance (classkey, p);
						}
					}  else {
						lua_pop (L, 2);
					}
				}
				break;
			case LUA_TNIL:
			case LUA_TTABLE:
			case LUA_TFUNCTION:
			case LUA_TLIGHTUSERDATA:
			default:
				std::cout << "  " << i << ": TYPE=" << t << ": " << lua_topointer(L, -2)<< "\n";
				break;
		}
		lua_pop (L, 2);
	}
	lua_pop (L, 1);
	lua_pop (L, 2);

	luabridge::push (L, rv);
	return 1;
}
#endif

LuaTableRef globalref;

int runone (LuaState& lua)
{
#if 0
	LuaState lua (*new LuaState);
#elif 0
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
		.addFunction ("pointer", &A::pointer)
		.addFunction ("minone", &A::minone)
		.addConst ("cologne", 4711)
		.endClass ()
		.addConst ("koln", 4711)
		.endNamespace ();
#endif
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Dump")

		.beginClass <LuaTableRef> ("TableRef")
		.addCFunction ("get", &LuaTableRef::get)
		.addCFunction ("set", &LuaTableRef::set)
		.endClass ()

		//.addCFunction ("dump", tableSerialize)
		.endNamespace ();

	luabridge::push <LuaTableRef *> (L, &globalref);
	lua_setglobal (L, "ref");


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
	add_history("t = {} t[2] = 7; t[3] = Test:A() t[4] = Test:A() ref:set (t);  f = ref:get()");

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

int main (int argc, char **argv)
{
	LuaState lua1;
	LuaState lua2;
	runone (lua1);
	printf ("=====\n");
	runone (lua2);
}
