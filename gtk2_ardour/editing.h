#ifndef __gtk_ardour_editing_h__
#define __gtk_ardour_editing_h__

#include <string>
#include <map>
#include <ardour/types.h>

// This involves some cpp magic. --taybin

#define SNAPTYPE(a) /*empty*/
#define SNAPMODE(a) /*empty*/
#define REGIONLISTSORTTYPE(a) /*empty*/
#define MOUSEMODE(a) /*empty*/
#define ZOOMFOCUS(a) /*empty*/
#define DISPLAYCONTROL(a) /*empty*/
#define COLORID(a) /*empty*/

namespace Editing {

// SNAPTYPE
#undef SNAPTYPE
#define SNAPTYPE(a) a,
enum SnapType {
	#include "editing_syms.h"
};

#undef SNAPTYPE
#define SNAPTYPE(s) #s,
static const char *snaptypestrs[] = {
	#include "editing_syms.h"
};
inline const char* enum2str(SnapType m) {return snaptypestrs[m];}
SnapType str2snaptype(ARDOUR::stringcr_t);

#undef SNAPTYPE
#define SNAPTYPE(a) /*empty*/

// SNAPMODE
#undef SNAPMODE
#define SNAPMODE(a) a,
enum SnapMode {
	#include "editing_syms.h"
};

#undef SNAPMODE
#define SNAPMODE(s) #s,
static const char *snapmodestrs[] = {
	#include "editing_syms.h"
};
inline const char* enum2str(SnapMode m) {return snapmodestrs[m];}
SnapMode str2snapmode(ARDOUR::stringcr_t);

#undef SNAPMODE
#define SNAPMODE(a) /*empty*/

// REGIONLISTSORTTYPE
#undef REGIONLISTSORTTYPE
#define REGIONLISTSORTTYPE(a) a,
enum RegionListSortType {
	#include "editing_syms.h"
};

#undef REGIONLISTSORTTYPE
#define REGIONLISTSORTTYPE(s) #s,
static const char *regionlistsorttypestrs[] = {
	#include "editing_syms.h"
};
inline const char* enum2str(RegionListSortType m) {return regionlistsorttypestrs[m];}
RegionListSortType str2regionlistsorttype(ARDOUR::stringcr_t);

#undef REGIONLISTSORTTYPE
#define REGIONLISTSORTTYPE(a) /*empty*/

// MOUSEMODE
#undef MOUSEMODE
#define MOUSEMODE(a) a,
enum MouseMode {
	#include "editing_syms.h"
};

#undef MOUSEMODE
#define MOUSEMODE(s) #s,
static const char *mousemodestrs[] = {
	#include "editing_syms.h"
};
inline const char* enum2str(MouseMode m) {return mousemodestrs[m];}
MouseMode str2mousemode(ARDOUR::stringcr_t);

#undef MOUSEMODE
#define MOUSEMODE(a) /*empty*/

// ZOOMFOCUS
#undef ZOOMFOCUS
#define ZOOMFOCUS(a) a,
enum ZoomFocus {
	#include "editing_syms.h"
};

#undef ZOOMFOCUS
#define ZOOMFOCUS(s) #s,
static const char *zoomfocusstrs[] = {
	#include "editing_syms.h"
};
inline const char* enum2str(ZoomFocus m) {return zoomfocusstrs[m];}
ZoomFocus str2zoomfocus(ARDOUR::stringcr_t);

#undef ZOOMFOCUS
#define ZOOMFOCUS(a) /*empty*/

// DISPLAYCONTROL
#undef DISPLAYCONTROL
#define DISPLAYCONTROL(a) a,
enum DisplayControl {
	#include "editing_syms.h"
};

#undef DISPLAYCONTROL
#define DISPLAYCONTROL(s) #s,
static const char *displaycontrolstrs[] = {
	#include "editing_syms.h"
};
inline const char* enum2str(DisplayControl m) {return displaycontrolstrs[m];}
DisplayControl str2displaycontrol (ARDOUR::stringcr_t);

#undef DISPLAYCONTROL
#define DISPLAYCONTROL(a) /*empty*/

#undef COLORID
#define COLORID(a) a,
enum ColorID {
	 #include "editing_syms.h"
};

#undef COLORID
#define COLORID(s) #s,
static const char *color_id_strs[] = {
	#include "editing_syms.h"
};
inline const char* enum2str(ColorID m) {return color_id_strs[m];}
ColorID str2color_id (ARDOUR::stringcr_t);

#undef COLORID
#define COLORID(a) /*empty*/

/////////////////////
// These don't need their state saved. yet...
enum CutCopyOp {
	Cut,
	Copy,
	Clear
};

enum XFadeType {
	Pre,
	Post,
	At
};

struct Color {
    char r;
    char g;
    char b;
    char a;
};

typedef std::map<Editing::ColorID,int> ColorMap;
extern ColorMap color_map;

} // namespace Editing

#endif // __gtk_ardour_editing_h__
