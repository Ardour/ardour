/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_surfaces_fp8controls_h_
#define _ardour_surfaces_fp8controls_h_

#include <map>

#include "fp8_base.h"
#include "fp8_button.h"
#include "fp8_strip.h"

namespace ArdourSurface { namespace FP_NAMESPACE {

class FP8Controls
{
public:
	FP8Controls (FP8Base&);
	virtual ~FP8Controls ();

	enum ButtonId {
		BtnPlay,
		BtnStop,
		BtnRecord,
		BtnLoop,
		BtnRewind,
		BtnFastForward,

		BtnALatch,
		BtnATrim,
		BtnAOff,
		BtnATouch,
		BtnAWrite,
		BtnARead,

		// Automation
		BtnSave,
		BtnRedo,
		BtnUndo,
		BtnUser1,
		BtnUser2,
		BtnUser3,

#ifndef FADERPORT2
		BtnFootswitch,
#endif

		// Pan/Param encoder press
		BtnParam,

		// Navigation
		BtnPrev,
		BtnNext,
		BtnEncoder,

		BtnChannel,
		BtnZoom,
		BtnScroll,
		BtnBank,
		BtnMaster,
		BtnClick,
		BtnSection,
		BtnMarker,

		BtnF1, BtnF2, BtnF3, BtnF4,
		BtnF5, BtnF6, BtnF7, BtnF8,
#ifdef FADERPORT2
		BtnFootswitch,
#endif

		// FaderMode
		BtnTrack,
		BtnPlugins,
		BtnSend,
		BtnPan,

		BtnTimecode,

		// Mix Management
		BtnMAudio,
		BtnMVI,
		BtnMBus,
		BtnMVCA,
		BtnMAll,

		BtnMInputs,
		BtnMMIDI,
		BtnMOutputs,
		BtnMFX,
		BtnMUser,

		// General Controls
		BtnArm,
		BtnArmAll,
		BtnSoloClear,
		BtnMuteClear,

		BtnBypass,
		BtnBypassAll,
		BtnMacro,
		BtnOpen,
		BtnLink,
		BtnLock,

		/* FP2 only */
		BtnChanLock,
		BtnFlip

	};

	typedef std::map <ButtonId, std::string> UserButtonMap;

	UserButtonMap const& user_buttons () const {
		return _user_buttons;
	}

	bool button_name_to_enum (std::string const&, ButtonId&) const;
	bool button_enum_to_name (ButtonId, std::string&) const;

	PBD::Signal0<void> FaderModeChanged;
	PBD::Signal0<void> MixModeChanged;

#ifdef FADERPORT2
	FP8Types::FaderMode fader_mode () const { return FP8Types::ModeTrack; }
	FP8Types::MixMode mix_mode () const { return FP8Types::MixUser; }
#else
	FP8Types::FaderMode fader_mode () const { return _fadermode; }
	FP8Types::MixMode mix_mode () const { return _mixmode; }
#endif

	FP8Types::NavigationMode nav_mode () const { return _navmode; }
	bool display_timecode () const { return _display_timecode; }

	FP8ButtonInterface& button (ButtonId id);
	FP8Strip& strip (uint8_t id);

	bool midi_event (uint8_t id, uint8_t val);
	bool midi_touch (uint8_t id, uint8_t val);
	bool midi_fader (uint8_t id, unsigned short val);

	void initialize ();
	void all_lights_off () const;

	void set_fader_mode (FP8Types::FaderMode);
protected:
	typedef std::map <uint8_t, FP8ButtonInterface*> MidiButtonMap;
	typedef std::map <ButtonId, FP8ButtonInterface*> CtrlButtonMap;

	void set_nav_mode (FP8Types::NavigationMode);
	void set_mix_mode (FP8Types::MixMode);
	void toggle_timecode ();

	MidiButtonMap _midimap;
	CtrlButtonMap _ctrlmap;
	MidiButtonMap _midimap_strip;

	FP8Strip* chanstrip[N_STRIPS];

	FP8Types::FaderMode      _fadermode;
	FP8Types::NavigationMode _navmode;
	FP8Types::MixMode        _mixmode;
	bool                     _display_timecode;

	UserButtonMap  _user_buttons;
	FP8DummyButton _dummy_button;

	std::map<std::string, ButtonId> _user_str_to_enum;
	std::map<ButtonId, std::string> _user_enum_to_str;

	PBD::ScopedConnectionList button_connections;
};

} } /* namespace */
#endif /* _ardour_surfaces_fp8controls_h_ */
