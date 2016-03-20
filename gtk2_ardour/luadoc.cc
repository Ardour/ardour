#include <stdio.h>
#include <string.h>
#include <iostream>

#include "ardour/luabindings.h"
#include "luainstance.h"

int main (int argc, char **argv)
{
	luabridge::setPrintBindings (true);
	LuaState lua;
	lua_State* L = lua.getState ();
#ifdef LUADOCOUT
	printf ("doc = {\n");
#else
	printf ("[\n");
#endif
	LuaInstance::register_classes (L);
	ARDOUR::LuaBindings::dsp (L);
#ifdef LUADOCOUT
	printf ("}\n");
#else
	printf ("{} ]\n");
#endif
	return 0;
}
