#include <stdio.h>
#include <string.h>
#include <iostream>

#include "ardour/luabindings.h"
#include "ardour/revision.h"
#include "luainstance.h"
#include "LuaBridge/LuaBridge.h"

#ifdef WAF_BUILD
#include "gtk2ardour-version.h"
#endif

int main (int argc, char **argv)
{
#ifdef LUABINDINGDOC
	luabridge::setPrintBindings (true);
	LuaState lua;
	lua_State* L = lua.getState ();
#ifdef LUADOCOUT
	printf ("-- %s\n", ARDOUR::revision);
	printf ("doc = {\n");
#else
	printf ("[\n");
	printf ("{\"version\" :  \"%s\"},\n\n", ARDOUR::revision);
#endif
	LuaInstance::register_classes (L);
	ARDOUR::LuaBindings::dsp (L);
#ifdef LUADOCOUT
	printf ("}\n");
#else
	printf ("{} ]\n");
#endif
	return 0;
#else
	return 1;
#endif
}
