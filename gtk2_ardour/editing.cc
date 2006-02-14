#include <string>

#include "editing.h"

using namespace std;

// This involves some cpp magic. --taybin

#define SNAPTYPE(a) /*empty*/
#define SNAPMODE(a) /*empty*/
#define REGIONLISTSORTTYPE(a) /*empty*/
#define MOUSEMODE(a) /*empty*/
#define ZOOMFOCUS(a) /*empty*/
#define DISPLAYCONTROL(a) /*empty*/

namespace Editing {

// SNAPTYPE
#undef SNAPTYPE
#define SNAPTYPE(s) if (!strcmp(type, #s)) {return s;}
SnapType
str2snaptype (const string & str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return SnapToBar;
}
#undef SNAPTYPE
#define SNAPTYPE(a) /*empty*/

// SNAPMODE
#undef SNAPMODE
#define SNAPMODE(s) if (!strcmp(type, #s)) {return s;}
SnapMode
str2snapmode (const string & str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return SnapNormal;
}
#undef SNAPMODE
#define SNAPMODE(a) /*empty*/

// REGIONLISTSORTTYPE
#undef REGIONLISTSORTTYPE
#define REGIONLISTSORTTYPE(s) if (!strcmp(type, #s)) {return s;}
RegionListSortType
str2regionlistsorttype (const string & str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return ByName;
}
#undef REGIONLISTSORTTYPE
#define REGIONLISTSORTTYPE(a) /*empty*/

// MOUSEMODE
#undef MOUSEMODE
#define MOUSEMODE(s) if (!strcmp(type, #s)) {return s;}
MouseMode
str2mousemode (const string & str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return MouseObject;
}
#undef MOUSEMODE
#define MOUSEMODE(a) /*empty*/

// ZOOMFOCUS
#undef ZOOMFOCUS
#define ZOOMFOCUS(s) if (!strcmp(type, #s)) {return s;}
ZoomFocus
str2zoomfocus (const string & str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return ZoomFocusPlayhead;
}
#undef ZOOMFOCUS
#define ZOOMFOCUS(a) /*empty*/

// DISPLAYCONTROL
#undef DISPLAYCONTROL
#define DISPLAYCONTROL(s) if (!strcmp(type, #s)) {return s;}
DisplayControl
str2displaycontrol (const string & str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return FollowPlayhead;
}
#undef DISPLAYCONTROL
#define DISPLAYCONTROL(a) /*empty*/

} // namespace Editing
