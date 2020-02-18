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

#include "fp8_controls.h"

using namespace ArdourSurface::FP_NAMESPACE;
using namespace ArdourSurface::FP_NAMESPACE::FP8Types;

bool FP8ButtonInterface::force_change = false;

#define NEWBUTTON(midi_id, button_id, color)            \
  do {                                                  \
  assert (_midimap.end() == _midimap.find (midi_id));   \
  assert (_ctrlmap.end() == _ctrlmap.find (button_id)); \
  FP8Button *t = new FP8Button (b, midi_id);            \
  _midimap[midi_id] = t;                                \
  _ctrlmap[button_id] = t;                              \
  } while (0)


#define NEWTYPEBUTTON(TYPE, midi_id, button_id, color)  \
  do {                                                  \
  assert (_midimap.end() == _midimap.find (midi_id));   \
  assert (_ctrlmap.end() == _ctrlmap.find (button_id)); \
  TYPE *t = new TYPE (b, midi_id);                      \
  _midimap[midi_id] = t;                                \
  _ctrlmap[button_id] = t;                              \
  } while (0)



#define NEWSHIFTBUTTON(midi_id, id1, id2, color)        \
  do {                                                  \
  assert (_midimap.end() == _midimap.find (midi_id));   \
  assert (_ctrlmap.end() == _ctrlmap.find (id1));       \
  assert (_ctrlmap.end() == _ctrlmap.find (id2));       \
  FP8ShiftSensitiveButton *t =                          \
    new FP8ShiftSensitiveButton (b, midi_id, color);    \
  _midimap[midi_id] = t;                                \
  _ctrlmap[id1] = t->button ();                         \
  _ctrlmap[id2] = t->button_shift ();                   \
  } while (0)


FP8Controls::FP8Controls (FP8Base& b)
	: _fadermode (ModeTrack)
#ifdef FADERPORT2
	, _navmode (NavScroll)
#else
	, _navmode (NavMaster)
#endif
	, _mixmode (MixAll)
	, _display_timecode (false)
{
	NEWBUTTON (0x56, BtnLoop, false);
	NEWTYPEBUTTON (FP8RepeatButton, 0x5b, BtnRewind, false);
	NEWTYPEBUTTON (FP8RepeatButton, 0x5c, BtnFastForward, false);
	NEWBUTTON (0x5d, BtnStop, false);
	NEWBUTTON (0x5e, BtnPlay, false);
	NEWBUTTON (0x5f, BtnRecord, false);

#ifdef FADERPORT2

	NEWSHIFTBUTTON (0x4a, BtnARead, BtnAOff, true);
	NEWSHIFTBUTTON (0x4b, BtnAWrite, BtnATrim, true);
	NEWSHIFTBUTTON (0x4d, BtnATouch, BtnALatch, true);

	NEWSHIFTBUTTON (0x2e, BtnPrev, BtnUndo, false);
	NEWSHIFTBUTTON (0x2f, BtnNext, BtnRedo, false);

	NEWSHIFTBUTTON (0x2a, BtnPan, BtnFlip, true);  //TODO: Flip Pan knob to fader ...?

	NEWSHIFTBUTTON (0x36, BtnChannel, BtnChanLock, true);

	NEWSHIFTBUTTON (0x38, BtnScroll,  BtnZoom, true);

	NEWSHIFTBUTTON (0x3a, BtnMaster,  BtnF1, false);
	NEWSHIFTBUTTON (0x3b, BtnClick,   BtnF2, false);
	NEWSHIFTBUTTON (0x3c, BtnSection, BtnF3, false);
	NEWSHIFTBUTTON (0x3d, BtnMarker,  BtnF4, false);

	//these buttons do not exist in FP2, but they need to exist in the ctrlmap:
	NEWBUTTON (0x71, BtnBank, false);
	NEWBUTTON (0x72, BtnF5, false);
	NEWBUTTON (0x73, BtnF6, false);
	NEWBUTTON (0x74, BtnF7, false);
	NEWBUTTON (0x75, BtnF8, false);
	NEWBUTTON (0x76, BtnUser1, false);
	NEWBUTTON (0x77, BtnUser2, false);
	NEWBUTTON (0x78, BtnUser3, false);
	NEWBUTTON (0x79, BtnSave, false);

#else
	NEWSHIFTBUTTON (0x4a, BtnARead, BtnUser3, true);
	NEWSHIFTBUTTON (0x4b, BtnAWrite, BtnUser2, true);
	NEWSHIFTBUTTON (0x4c, BtnATrim, BtnRedo, true);
	NEWSHIFTBUTTON (0x4d, BtnATouch, BtnUser1, true);
	NEWSHIFTBUTTON (0x4e, BtnALatch, BtnSave, true);
	NEWSHIFTBUTTON (0x4f, BtnAOff, BtnUndo, true);

	NEWBUTTON (0x2e, BtnPrev, false);
	NEWBUTTON (0x2f, BtnNext, false);

	NEWSHIFTBUTTON (0x36, BtnChannel, BtnF1, false);
	NEWSHIFTBUTTON (0x37, BtnZoom,    BtnF2, false);
	NEWSHIFTBUTTON (0x38, BtnScroll,  BtnF3, false);
	NEWSHIFTBUTTON (0x39, BtnBank,    BtnF4, false);
	NEWSHIFTBUTTON (0x3a, BtnMaster,  BtnF5, false);
	NEWSHIFTBUTTON (0x3b, BtnClick,   BtnF6, false);
	NEWSHIFTBUTTON (0x3c, BtnSection, BtnF7, false);
	NEWSHIFTBUTTON (0x3d, BtnMarker,  BtnF8, false);

	NEWBUTTON (0x2a, BtnPan, false);
#endif

	NEWSHIFTBUTTON (0x28, BtnTrack, BtnTimecode, false);
	NEWBUTTON (0x2b, BtnPlugins, false);
	NEWBUTTON (0x29, BtnSend, false);

	NEWSHIFTBUTTON (0x00, BtnArm, BtnArmAll, false);
	NEWBUTTON (0x01, BtnSoloClear, false);
	NEWBUTTON (0x02, BtnMuteClear, false);

	NEWSHIFTBUTTON (0x03, BtnBypass, BtnBypassAll, true);
	NEWSHIFTBUTTON (0x04, BtnMacro, BtnOpen, true);
	NEWSHIFTBUTTON (0x05, BtnLink, BtnLock, true);

	NEWSHIFTBUTTON (0x3e, BtnMAudio, BtnMInputs, true);
	NEWSHIFTBUTTON (0x3f, BtnMVI, BtnMMIDI, true);
	NEWSHIFTBUTTON (0x40, BtnMBus, BtnMOutputs, true);
	NEWSHIFTBUTTON (0x41, BtnMVCA, BtnMFX, true);
	NEWSHIFTBUTTON (0x42, BtnMAll, BtnMUser, true);

	NEWTYPEBUTTON (FP8ReadOnlyButton, 0x53, BtnEncoder, false);
	NEWTYPEBUTTON (FP8ReadOnlyButton, 0x20, BtnParam, false);
	NEWTYPEBUTTON (FP8ReadOnlyButton, 0x66, BtnFootswitch, false);

	/* internal bindings */

#define BindMethod(ID, CB) \
	button (ID).released.connect_same_thread (button_connections, boost::bind (&FP8Controls:: CB, this));

	BindMethod (FP8Controls::BtnTimecode, toggle_timecode);

#define BindNav(BTN, MODE)\
	button (BTN).released.connect_same_thread (button_connections, boost::bind (&FP8Controls::set_nav_mode, this, MODE))

	BindNav (BtnChannel, NavChannel);
	BindNav (BtnZoom,    NavZoom);
	BindNav (BtnScroll,  NavScroll);
	BindNav (BtnBank,    NavBank);
	BindNav (BtnMaster,  NavMaster);
	BindNav (BtnSection, NavSection);
	BindNav (BtnMarker,  NavMarker);
#ifdef FADERPORT2
	BindNav (BtnPan,     NavPan);
#endif

#define BindFader(BTN, MODE)\
	button (BTN).released.connect_same_thread (button_connections, boost::bind (&FP8Controls::set_fader_mode, this, MODE))

	BindFader (BtnTrack,   ModeTrack);
	BindFader (BtnPlugins, ModePlugins);
	BindFader (BtnSend,    ModeSend);
#ifndef FADERPORT2
	BindFader (BtnPan,     ModePan);
#endif


#define BindMix(BTN, MODE)\
	button (BTN).released.connect_same_thread (button_connections, boost::bind (&FP8Controls::set_mix_mode, this, MODE))

	BindMix (BtnMAudio,   MixAudio);
	BindMix (BtnMVI,      MixInstrument);
	BindMix (BtnMBus,     MixBus);
	BindMix (BtnMVCA,     MixVCA);
	BindMix (BtnMAll,     MixAll);
	BindMix (BtnMInputs,  MixInputs);
	BindMix (BtnMMIDI,    MixMIDI);
	BindMix (BtnMOutputs, MixOutputs);
	BindMix (BtnMFX,      MixFX);
	BindMix (BtnMUser,    MixUser);

	/* create channelstrips */
	for (uint8_t id = 0; id < N_STRIPS; ++id) {
		chanstrip[id] = new FP8Strip (b, id);
		_midimap_strip[FP8Strip::midi_ctrl_id (FP8Strip::BtnSolo, id)] = &(chanstrip[id]->solo_button());
		_midimap_strip[FP8Strip::midi_ctrl_id (FP8Strip::BtnMute, id)] = &(chanstrip[id]->mute_button());
		_midimap_strip[FP8Strip::midi_ctrl_id (FP8Strip::BtnSelect, id)] = &(chanstrip[id]->selrec_button());
	}

	/* set User button names */

#define REGISTER_ENUM(ID, NAME) \
	_user_str_to_enum[#ID] = ID; \
	_user_enum_to_str[ID]  = #ID; \
	_user_buttons[ID]      = NAME;

#ifdef FADERPORT2
	REGISTER_ENUM (BtnF1        , "F1");
	REGISTER_ENUM (BtnF2        , "F2");
	REGISTER_ENUM (BtnF3        , "F3");
	REGISTER_ENUM (BtnF4        , "F4");
	REGISTER_ENUM (BtnFootswitch, "Footswitch");
#else
	REGISTER_ENUM (BtnFootswitch, "Footswitch");
	REGISTER_ENUM (BtnUser1     , "User 1");
	REGISTER_ENUM (BtnUser2     , "User 2");
	REGISTER_ENUM (BtnUser3     , "User 3");
	REGISTER_ENUM (BtnF1        , "F1");
	REGISTER_ENUM (BtnF2        , "F2");
	REGISTER_ENUM (BtnF3        , "F3");
	REGISTER_ENUM (BtnF4        , "F4");
	REGISTER_ENUM (BtnF5        , "F5");
	REGISTER_ENUM (BtnF6        , "F6");
	REGISTER_ENUM (BtnF7        , "F7");
	REGISTER_ENUM (BtnF8        , "F8");
#endif

#undef REGISTER_ENUM
}

FP8Controls::~FP8Controls ()
{
	for (MidiButtonMap::const_iterator i = _midimap.begin (); i != _midimap.end (); ++i) {
		delete i->second;
	}
	for (uint8_t id = 0; id < N_STRIPS; ++id) {
		delete chanstrip[id];
	}
	_midimap_strip.clear ();
	_ctrlmap.clear ();
	_midimap.clear ();
}

bool
FP8Controls::button_name_to_enum (std::string const& n, ButtonId& id) const
{
	std::map<std::string, ButtonId>::const_iterator i = _user_str_to_enum.find (n);
	if (i == _user_str_to_enum.end()) {
		return false;
	}
	id = i->second;
	return true;
}

bool
FP8Controls::button_enum_to_name (ButtonId id, std::string& n) const
{
	std::map<ButtonId, std::string>::const_iterator i = _user_enum_to_str.find (id);
	if (i == _user_enum_to_str.end()) {
		return false;
	}
	n = i->second;
	return true;
}

void
FP8Controls::initialize ()
{
	FP8ButtonInterface::force_change = true;
	/* set RGB colors */
	button (BtnUndo).set_color (0x00ff00ff);
	button (BtnRedo).set_color (0x00ff00ff);

	button (BtnAOff).set_color (0xffffffff);
	button (BtnATrim).set_color (0x000030ff);
	button (BtnARead).set_color (0x00ff00ff);
	button (BtnAWrite).set_color (0xff0000ff);
	button (BtnATouch).set_color (0xff8800ff);
	button (BtnALatch).set_color (0xffff00ff);

	button (BtnUser1).set_color (0x0000ffff);
	button (BtnUser2).set_color (0x0000ffff);
	button (BtnUser3).set_color (0x0000ffff);

	button (BtnBypass).set_color (0x888888ff);
	button (BtnBypassAll).set_color (0xffffffff);

	button (BtnMacro).set_color (0x888888ff);
	button (BtnOpen).set_color (0xffffffff);

	button (BtnLink).set_color (0x888888ff);
	button (BtnLock).set_color (0xffffffff);

	button (BtnMAudio).set_color (0x0000ffff);
	button (BtnMVI).set_color (0x0000ffff);
	button (BtnMBus).set_color (0x0000ffff);
	button (BtnMVCA).set_color (0x0000ffff);
	button (BtnMAll).set_color (0x0000ffff);

	button (BtnMInputs).set_color (0x0000ffff);
	button (BtnMMIDI).set_color (0x0000ffff);
	button (BtnMOutputs).set_color (0x0000ffff);
	button (BtnMFX).set_color (0x0000ffff);
	button (BtnMUser).set_color (0x0000ffff);

#ifdef FADERPORT2
	/* encoder mode-switches are orange, to match the Master switch physical color */
	button (BtnLink).set_color (0x000000ff);
	button (BtnChannel).set_color (0x0000ffff);
	button (BtnScroll).set_color (0x0000ffff);
	button (BtnPan).set_color (0xffffffff);
#endif

	for (uint8_t id = 0; id < N_STRIPS; ++id) {
		chanstrip[id]->initialize ();
	}

	/* initally turn all lights off */
	all_lights_off ();

	/* default modes */
#ifdef FADERPORT2
	button (BtnScroll).set_active (true);
#else
	button (BtnMaster).set_active (true);
#endif
	button (BtnTrack).set_active (true);
	button (BtnMAll).set_active (true);
	button (BtnTimecode).set_active (_display_timecode);

	FP8ButtonInterface::force_change = false;
}

void
FP8Controls::all_lights_off () const
{
	for (CtrlButtonMap::const_iterator i = _ctrlmap.begin (); i != _ctrlmap.end (); ++i) {
		i->second->set_active (false);
	}
}

FP8ButtonInterface&
FP8Controls::button (ButtonId id)
{
	CtrlButtonMap::const_iterator i = _ctrlmap.find (id);
	if (i == _ctrlmap.end()) {
		assert (0);
		return _dummy_button;
	}
	return *(i->second);
}

FP8Strip&
FP8Controls::strip (uint8_t id)
{
	assert (id < N_STRIPS);
	return *chanstrip[id];
}

/* *****************************************************************************
 * Delegate MIDI events
 */

bool
FP8Controls::midi_event (uint8_t id, uint8_t val)
{
	MidiButtonMap::const_iterator i;

	i = _midimap_strip.find (id);
	if (i != _midimap_strip.end()) {
		return i->second->midi_event (val > 0x40);
	}

	i = _midimap.find (id);
	if (i != _midimap.end()) {
		return i->second->midi_event (val > 0x40);
	}
	return false;
}

bool
FP8Controls::midi_touch (uint8_t id, uint8_t val)
{
	assert (id < N_STRIPS);
	return chanstrip[id]->midi_touch (val > 0x40);
}

bool
FP8Controls::midi_fader (uint8_t id, unsigned short val)
{
	assert (id < N_STRIPS);
	return chanstrip[id]->midi_fader ((val >> 4) / 1023.f);
}

/* *****************************************************************************
 * Internal Model + View for Modes
 */

void
FP8Controls::set_nav_mode (NavigationMode m)
{
	if (_navmode == m) {
		return;
	}
	// TODO add special-cases:
	// - master/monitor (blink when button is held + monitor section present)
	// - "click" hold -> encoder sets click volume, encoder-press toggle rec-only-metro
	button (BtnChannel).set_active (m == NavChannel);
	button (BtnZoom).set_active (m == NavZoom);
	button (BtnScroll).set_active (m == NavScroll);
	button (BtnBank).set_active (m == NavBank);
	button (BtnMaster).set_active (m == NavMaster);
	button (BtnSection).set_active (m == NavSection);
	button (BtnMarker).set_active (m == NavMarker);
#ifdef FADERPORT2
	button (BtnPan).set_active (m == NavPan);
#endif
	_navmode = m;
}

void
FP8Controls::set_fader_mode (FaderMode m)
{
	if (_fadermode == m) {
		if (m == ModePlugins || m == ModeSend) {
			/* "Edit Plugins" while editing Plugin-params, returns back
			 * to plugin selection.
			 * "Sends" button banks through sends.
			 */
			FaderModeChanged ();
		}
		return;
	}
	// set lights
	button (BtnTrack).set_active (m == ModeTrack);
	button (BtnPlugins).set_active (m == ModePlugins);
	button (BtnSend).set_active (m == ModeSend);
	button (BtnPan).set_active (m == ModePan);
	_fadermode = m;
	FaderModeChanged ();
}

void
FP8Controls::set_mix_mode (MixMode m)
{
	if (_mixmode == m) {
		if (m == MixUser || m == MixInputs) {
			/* always re-assign:
			 *  - MixUser: depends on selection
			 *  - MixInputs: depends on rec-arm
			 */
			MixModeChanged ();
		}
		return;
	}
	button (BtnMAudio).set_active (m == MixAudio);
	button (BtnMVI).set_active (m == MixInstrument);
	button (BtnMBus).set_active (m == MixBus);
	button (BtnMVCA).set_active (m == MixVCA);
	button (BtnMAll).set_active (m == MixAll);
	button (BtnMInputs).set_active (m == MixInputs);
	button (BtnMMIDI).set_active (m == MixMIDI);
	button (BtnMOutputs).set_active (m == MixOutputs);
	button (BtnMFX).set_active (m == MixFX);
	button (BtnMUser).set_active (m == MixUser);

	_mixmode = m;
	MixModeChanged ();
}

void
FP8Controls::toggle_timecode ()
{
	_display_timecode = !_display_timecode;
	button (BtnTimecode).set_active (_display_timecode);
}
