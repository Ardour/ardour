/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include "m2_map_mk2.h"

using namespace ArdourSurface;
using namespace std;

M2MapMk2::M2MapMk2 ()
	: M2Contols ()
	, enc_master (16)
{
#define PSMAP(MOD, PHYS, SEM, BTN) \
	pmap[MOD].insert (make_pair (PHYS, BTN)); \
	smap.insert (make_pair (SEM, BTN));

#define PSMAPALL(PHYS, SEM, BTN) \
	pmap[ModNone].insert (make_pair (PHYS, BTN)); \
	pmap[ModShift].insert (make_pair (PHYS, BTN)); \
	smap.insert (make_pair (SEM, BTN)); \

	PSMAP(ModNone,  BtnPlay, Play,         &tr[0]);
	PSMAP(ModShift, BtnPlay, Metronom,     &tr[1]);
	PSMAP(ModNone,  BtnRec, Rec,           &tr[2]);
	PSMAP(ModNone,  BtnGrid, Grid,         &tr[3]);
	PSMAP(ModNone,  BtnRestart, GotoStart, &ts[0]);
	PSMAP(ModShift, BtnRestart, Loop,      &tr[4]);

	PSMAP(ModNone,  BtnStepLeft,  FastRewind,   &ts[1]);
	PSMAP(ModNone,  BtnStepRight, FastForward,  &ts[2]);
	PSMAP(ModShift, BtnStepLeft,  JumpBackward, &ts[3]);
	PSMAP(ModShift, BtnStepRight, JumpForward,  &ts[4]);

	PSMAPALL(BtnWheel,  EncoderWheel, &mst[0]);
	PSMAPALL(BtnVolume, MasterVolume, &mst[1]);
	//PSMAPALL(BtnSwing, Master?????, &mst[2]);
	PSMAPALL(BtnTempo,  MasterTempo,  &mst[3]);

	PSMAP(ModShift,  BtnAll, Save, &save);

	PSMAP(ModShift,  BtnNavLeft,  Undo, &undoredo[0]);
	PSMAP(ModShift,  BtnNavRight, Redo, &undoredo[1]);

	PSMAP(ModNone,  BtnMute, Mute,  &sm[0]);
	PSMAP(ModShift, BtnMute, Panic, &panic);
	PSMAPALL(BtnSolo, Solo,         &sm[1]);

	// TODO:
	pmap[ModNone].insert  (make_pair (BtnErase, &ts[5]));
	pmap[ModShift].insert (make_pair (BtnErase, &ts[5]));

}

M2ButtonInterface*
M2MapMk2::button (PhysicalButtonId id, Modifier m)
{
	PhysicalMap::const_iterator i = pmap[m].find (id);
	if (i != pmap[m].end()) {
		return i->second;
	}
	return M2Contols::button (id, m);
}

M2ButtonInterface*
M2MapMk2::button (SemanticButtonId id)
{
	SematicMap::const_iterator i = smap.find (id);
	if (i != smap.end()) {
		return i->second;
	}
	return M2Contols::button (id);
}

M2EncoderInterface*
M2MapMk2::encoder (unsigned int id)
{
	if (id == 0) {
		return &enc_master;
	}
	else if (id < 9) {
		return &enc_top[id - 1];
	}
	return M2Contols::encoder (id);
}

M2PadInterface*
M2MapMk2::pad (unsigned int id)
{
	if (id < 16) {
		return &pads[id];
	}
	return M2Contols::pad (id);
}
