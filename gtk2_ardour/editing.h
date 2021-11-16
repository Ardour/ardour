/*
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2009 David Robillard <d@drobilla.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk_ardour_editing_h__
#define __gtk_ardour_editing_h__

#include <string>
#include <map>
#include "ardour/types.h"

// This involves some cpp magic. --taybin

#define GRIDTYPE(a) /*empty*/
#define SNAPMODE(a) /*empty*/
#define REGIONLISTSORTTYPE(a) /*empty*/
#define MOUSEMODE(a) /*empty*/
#define MIDIEDITMODE(a) /*empty*/
#define ZOOMFOCUS(a) /*empty*/
#define DISPLAYCONTROL(a) /*empty*/
#define IMPORTMODE(a) /*empty*/
#define IMPORTPOSITION(a)
#define IMPORTDISPOSITION(a)
#define EDITPOINT(a) /*empty*/
#define WAVEFORMSCALE(a) /*empty*/
#define WAVEFORMSHAPE(a) /*empty*/
#define INSERTTIMEOPT(a) /*empty*/

namespace Editing {

// GRIDTYPE
#undef GRIDTYPE
#define GRIDTYPE(a) a,
enum GridType {
	#include "editing_syms.h"
};

static const int      DRAW_VEL_AUTO = -1;
static const int      DRAW_CHAN_AUTO = -1;
static const GridType DRAW_LEN_AUTO = GridTypeNone;  //special case: use the Grid's value instead of the note-length selection

extern const char *gridtypestrs[];
inline const char* enum2str(GridType m) {return gridtypestrs[m];}
GridType str2gridtype(const std::string &);

#undef GRIDTYPE
#define GRIDTYPE(a) /*empty*/

// SNAPMODE
#undef SNAPMODE
#define SNAPMODE(a) a,
enum SnapMode {
	#include "editing_syms.h"
};

extern const char *snapmodestrs[];
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

extern const char *regionlistsorttypestrs[];
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

extern const char *mousemodestrs[];
inline const char* enum2str(MouseMode m) {return mousemodestrs[m];}
MouseMode str2mousemode(const std::string &);

#undef MOUSEMODE
#define MOUSEMODE(a) /*empty*/

// MIDIEDITMODE
#undef MIDIEDITMODE
#define MIDIEDITMODE(a) a,
enum MidiEditMode {
	#include "editing_syms.h"
};

extern const char *midieditmodestrs[];
inline const char* enum2str(MidiEditMode m) {return midieditmodestrs[m];}
MidiEditMode str2midieditmode(const std::string &);

#undef MIDIEDITMODE
#define MIDIEDITMODE(a) /*empty*/

// ZOOMFOCUS
#undef ZOOMFOCUS
#define ZOOMFOCUS(a) a,
enum ZoomFocus {
	#include "editing_syms.h"
};

extern const char *zoomfocusstrs[];
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

extern const char *displaycontrolstrs[];
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
#define IMPORTMODE(a) /*empty*/

// IMPORTPOSITION
#undef IMPORTPOSITION
#define IMPORTPOSITION(a) a,
enum ImportPosition {
	#include "editing_syms.h"
};

#undef IMPORTPOSITION
#define IMPORTPOSITION(a) /*empty*/

// IMPORTDISPOSITION
#undef IMPORTDISPOSITION
#define IMPORTDISPOSITION(a) a,
enum ImportDisposition {
	#include "editing_syms.h"
};

#undef IMPORTDISPOSITION
#define IMPORTDISPOSITION(a) /*empty*/

// EDITPOINT
#undef EDITPOINT
#define EDITPOINT(a) a,
enum EditPoint {
	#include "editing_syms.h"
};

#undef EDITPOINT
#define EDITPOINT(a) /*empty*/

// INSERTTIMEOPT
#undef INSERTTIMEOPT
#define INSERTTIMEOPT(a) a,
enum InsertTimeOption {
	#include "editing_syms.h"
};

#undef INSERTTIMEOPT
#define INSERTTIMEOPT(a) /*empty*/


/////////////////////
// These don't need their state saved. yet...
enum CutCopyOp {
	Delete,
	Cut,
	Copy,
	Clear
};

enum XFadeType {
	Pre,
	Post,
	At
};

enum EditIgnoreOption {
	EDIT_IGNORE_NONE,
	EDIT_IGNORE_PHEAD,
	EDIT_IGNORE_MOUSE,
	EDIT_IGNORE_MARKER
};

enum ZoomAxis {
	Vertical,
	Horizontal,
	Both
};

enum RegionActionTarget {
	SelectedRegions = 0x1,
	EnteredRegions = 0x2,
	EditPointRegions = 0x4,
	ListSelection = 0x8
};

} // namespace Editing

#endif // __gtk_ardour_editing_h__
