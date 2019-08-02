/*
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cstring>

#include "editing.h"

#include "pbd/i18n.h"

using namespace std;

// This involves some cpp magic. --taybin

#define GRIDTYPE(a) /*empty*/
#define SNAPMODE(a) /*empty*/
#define REGIONLISTSORTTYPE(a) /*empty*/
#define MOUSEMODE(a) /*empty*/
#define MIDIEDITMODE(a) /*empty*/
#define ZOOMFOCUS(a) /*empty*/
#define DISPLAYCONTROL(a) /*empty*/

namespace Editing {

// GRIDTYPE
#undef GRIDTYPE
#define GRIDTYPE(s) if (!strcmp(type, #s)) {return s;}
GridType
str2gridtype (const string & str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return GridTypeBar;
}

#undef GRIDTYPE
#define GRIDTYPE(s) N_(#s),
const char *gridtypestrs[] = {
	#include "editing_syms.h"
	0
};
#undef GRIDTYPE
#define GRIDTYPE(a) /*empty*/

// SNAPMODE
#undef SNAPMODE
#define SNAPMODE(s) if (!strcmp(type, #s)) {return s;}
SnapMode
str2snapmode (const string & str) {
	const char* type = str.c_str();
	#include "editing_syms.h"
	return SnapMagnetic;
}

#undef SNAPMODE
#define SNAPMODE(s) N_(#s),
const char *snapmodestrs[] = {
	#include "editing_syms.h"
	0
};
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
#define REGIONLISTSORTTYPE(s) N_(#s),
const char *regionlistsorttypestrs[] = {
	#include "editing_syms.h"
	0
};
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
#define MOUSEMODE(s) N_(#s),
const char *mousemodestrs[] = {
	#include "editing_syms.h"
	0
};
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
#define ZOOMFOCUS(s) N_(#s),
const char *zoomfocusstrs[] = {
	#include "editing_syms.h"
	0
};
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
#define DISPLAYCONTROL(s) N_(#s),
const char *displaycontrolstrs[] = {
	#include "editing_syms.h"
	0
};
#undef DISPLAYCONTROL
#define DISPLAYCONTROL(a) /*empty*/

//IMPORTMODE
#undef IMPORTMODE
#define IMPORTMODE(s) N_(#s),
const char *importmodestrs[] = {
	#include "editing_syms.h"
	0
};
#undef IMPORTMODE
#define IMPORTMODE(a) /*empty*/

} // namespace Editing

