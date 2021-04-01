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

#ifndef _ardour_surfaces_m2controls_h_
#define _ardour_surfaces_m2controls_h_

#include <map>

#include "m2_button.h"
#include "m2_encoder.h"
#include "m2_pad.h"

namespace ArdourSurface {

/** Abstraction for various variants:
 *  - NI Maschine Mikro
 *  - NI Maschine
 *  - NI Maschine Studio
 */

class M2Contols
{
	public:
		M2Contols () {}
		virtual ~M2Contols () {}

		typedef enum {
			ModNone = 0,
			ModShift,
		} Modifier;

		typedef enum {
			/* Transport */
			BtnRestart,
			BtnStepLeft,
			BtnStepRight,
			BtnGrid,
			BtnPlay,
			BtnRec,
			BtnErase,
			BtnShift,

			/* modes */
			BtnScene,
			BtnPattern,
			BtnPadMode,
			BtnNavigate, // aka. "view" on Mikro
			BtnDuplicate,
			BtnSelect,
			BtnSolo,
			BtnMute,

			/* global */
#if 0
			BtnArrange, // Studio only
			BtnMix,     // Studio only
#endif

			BtnControl, // Studio: "Channel"
			BtnStep,    // Studio: "Plug-In"
			BtnBrowse,
			BtnSampling,
			BtnSelLeft,
			BtnSelRight,
			BtnAll,
			BtnAuto,

			/* master */
			BtnVolume,
			BtnSwing,
			BtnTempo,
			BtnNavLeft,
			BtnNavRight,
			BtnEnter,
			BtnNoteRepeat, // Tap
			BtnWheel, // Encoder Push

			/* Selectors above display */
			BtnTop0, BtnTop1, BtnTop2, BtnTop3, // Mikro F1, F2, F3
			BtnTop4, BtnTop5, BtnTop6, BtnTop7,

			/* Maschine & Studio "Groups" */
			BtnGroupA, BtnGroupB, BtnGroupC, BtnGroupD,
			BtnGroupE, BtnGroupF, BtnGroupG, BtnGroupH,

#if 1 // Studio only -- Edit
			BtnCopy,
			BtnPaste,
			BtnNote,
			BtnNudge,
			BtnUndo,
			BtnRedo,
			BtnQuantize,
			BtnClear,

			BtnIn1, BtnIn2, BtnIn3, BtnIn4,
			BtnMst, BtnGrp, BtnSnd, BtnCue,
#endif
		} PhysicalButtonId;

		typedef enum {
			Play,
			Rec,
			Loop,
			Metronom,
			GotoStart,
			GotoEnd,
			JumpBackward,
			JumpForward,
			FastRewind,
			FastForward,
			Grid,
			Delete,
			Undo, Redo,
			Save,
			EncoderWheel, // multi-purpose
			MasterVolume,
			MasterTempo,
			Scene,
			Pattern,
			PadMode,
			Navigate,
			Duplicate,
			Select,
			Solo,
			Mute,
			Panic
		} SemanticButtonId;

		typedef std::map <PhysicalButtonId, M2ButtonInterface*> PhysicalMap;
		typedef std::map <SemanticButtonId, M2ButtonInterface*> SematicMap;

		virtual M2ButtonInterface* button (PhysicalButtonId id, Modifier m) {
			if (id == BtnShift) {
				return &_shift;
			}
			return &_dummy_button;
		}

		virtual M2ButtonInterface* button (SemanticButtonId id) {
			return &_dummy_button;
		}

		virtual M2EncoderInterface* encoder (unsigned int id) {
			return &_dummy_encoder;
		}

		virtual M2PadInterface* pad (unsigned int id) {
			return &_dummy_pad;
		}

	protected:
		M2ButtonInterface  _dummy_button;
		M2EncoderInterface _dummy_encoder;
		M2PadInterface     _dummy_pad;

		M2ToggleHoldButton  _shift;
};

} /* namespace */
#endif /* _ardour_surfaces_m2controls_h_*/
