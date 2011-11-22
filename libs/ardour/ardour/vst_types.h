/*
    Copyright (C) 2010 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_vst_types_h__
#define __ardour_vst_types_h__

#include "ardour/vestige/aeffectx.h"

struct _VSTKey
{
	/** virtual-key code, or 0 if this _VSTFXKey is a `character' key */
	int special;
	/** `character' key, or 0 if this _VSTFXKey is a virtual-key */
	int character;
};

typedef struct _VSTKey VSTKey;

struct _VSTInfo 
{
	char  *name;
	char  *creator;
	int    UniqueID;
	char  *Category;
    
	int    numInputs;
	int    numOutputs;
	int    numParams;
	
	int    wantMidi;
	int    wantEvents;
	int    hasEditor;
	int    canProcessReplacing;
	
	char** ParamNames;
	char** ParamLabels;
};

typedef struct _VSTInfo VSTInfo;

typedef AEffect * (* main_entry_t) (audioMasterCallback);

struct _VSTHandle
{
	void*        dll;
	char*        name;
	char*        nameptr;
	
	main_entry_t main_entry;

	int          plugincnt;
};

typedef struct _VSTHandle VSTHandle;

#endif
