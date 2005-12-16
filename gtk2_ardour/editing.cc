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
str2snaptype (ARDOUR::stringcr_t str) {
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
str2snapmode (ARDOUR::stringcr_t str) {
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
str2regionlistsorttype (ARDOUR::stringcr_t str) {
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
str2mousemode (ARDOUR::stringcr_t str) {
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
str2zoomfocus (ARDOUR::stringcr_t str) {
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
str2displaycontrol (ARDOUR::stringcr_t str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return FollowPlayhead;
}
#undef DISPLAYCONTROL
#define DISPLAYCONTROL(a) /*empty*/

// COLORID
#undef COLORID
#define COLORID(s) if (!strcmp(type, #s)) {return s;}
ColorID
str2color_id (ARDOUR::stringcr_t str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return cFrameHandleEndOutline;
}
#undef COLORID
#define COLORID(a) /*empty*/

ColorMap color_map;

} // namespace Editing
