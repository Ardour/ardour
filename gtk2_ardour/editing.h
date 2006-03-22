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
#define IMPORTMODE(a) /*empty*/

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
SnapType str2snaptype(const std::string &);

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
SnapMode str2snapmode(const std::string &);

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
RegionListSortType str2regionlistsorttype(const std::string &);

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
MouseMode str2mousemode(const std::string &);

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
ZoomFocus str2zoomfocus(const std::string &);

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
DisplayControl str2displaycontrol (const std::string &);

#undef DISPLAYCONTROL
#define DISPLAYCONTROL(a) /*empty*/

// IMPORTMODE
#undef IMPORTMODE
#define IMPORTMODE(a) a,
enum ImportMode {
	#include "editing_syms.h"
};

#undef IMPORTMODE
#define IMPORTMODE(s) #s,
static const char *importmodestrs[] = {
	#include "editing_syms.h"
};
inline const char* enum2str(ImportMode m) {return importmodestrs[m];}
ImportMode str2importmode (const std::string &);

#undef IMPORTMODE
#define IMPORTMODE(a) /*empty*/

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

} // namespace Editing

#endif // __gtk_ardour_editing_h__
