/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_surfaces_fp8base_h_
#define _ardour_surfaces_fp8base_h_

#include <stdint.h>
#include <vector>

#include "pbd/signals.h"

#ifdef FADERPORT16
# define FP_NAMESPACE FP16
#elif defined FADERPORT2
# define FP_NAMESPACE FP2
#else
# define FP_NAMESPACE FP8
#endif

namespace ArdourSurface { namespace FP_NAMESPACE {

/* conveniece wrappers depending on "FP8Base& _base" */
#define fp8_loop dynamic_cast<BaseUI*>(&_base)->main_loop
#define fp8_context() dynamic_cast<BaseUI*>(&_base)
#define fp8_protocol() dynamic_cast<ControlProtocol*>(&_base)

/** Virtual abstract base of the FaderPort8 control surface
 *
 * This is passed as handle to all elements (buttons, lights,..)
 * to inteface common functionality for the current instance:
 *  - sending MIDI
 *  - global events (signals)
 *  - thread context
 *
 * It is implemented by FaderPort8
 */
class FP8Base
{
public:
	virtual ~FP8Base() {}

	virtual size_t tx_midi (std::vector<uint8_t> const&) const = 0;
	virtual std::string const& timecode () const = 0;
	virtual std::string const& musical_time () const = 0;
	virtual bool shift_mod () const = 0;
	virtual bool show_meters () const = 0;
	virtual bool show_panner () const = 0;
	virtual bool twolinetext () const = 0;
	virtual uint32_t clock_mode () const = 0;

	size_t tx_midi2 (uint8_t sb, uint8_t d1) const
	{
		 std::vector<uint8_t> d;
		 d.push_back (sb);
		 d.push_back (d1);
		 return tx_midi (d);
	}

	size_t tx_midi3 (uint8_t sb, uint8_t d1, uint8_t d2) const
	{
		 std::vector<uint8_t> d;
		 d.push_back (sb);
		 d.push_back (d1);
		 d.push_back (d2);
		 return tx_midi (d);
	}

	size_t tx_sysex (size_t count, ...)
	{
		 std::vector<uint8_t> d;
		 sysexhdr (d);

		 va_list var_args;
		 va_start (var_args, count);
		 for  (size_t i = 0; i < count; ++i)
		 {
			 // uint8_t {aka unsigned char} is promoted to ‘int’ when passed through ‘...’
			 uint8_t b = va_arg (var_args, int);
			 d.push_back (b);
		 }
		 va_end (var_args);

		 d.push_back (0xf7);
		 return tx_midi (d);
	}

	size_t tx_text (uint8_t id, uint8_t line, uint8_t align, std::string const& txt)
	{
		 std::vector<uint8_t> d;
		 sysexhdr (d);
		 d.push_back (0x12);
		 d.push_back (id & 0x0f);
		 d.push_back (line & 0x03);
		 d.push_back (align & 0x07);

		 for  (size_t i = 0; i < txt.size(); ++i)
		 {
			 if (txt[i] < 0) {
				 continue;
			 }
			 d.push_back (txt[i]);
			 if (i >= 8) {
				 break;
			 }
		 }
		 d.push_back (0xf7);
		 return tx_midi (d);
	}

	/* modifier keys */
	PBD::Signal1<void, bool> ShiftButtonChange;
	PBD::Signal1<void, bool> ARMButtonChange;

	/* timer events */
	PBD::Signal1<void, bool> BlinkIt;
	PBD::Signal0<void> Periodic;

private:
	void sysexhdr (std::vector<uint8_t>& d)
	{
		/* faderport8 <SysExHdr> */
		d.push_back (0xf0);
		d.push_back (0x00);
		d.push_back (0x01);
		d.push_back (0x06);
#ifdef FADERPORT16
		d.push_back (0x16);
#else
		d.push_back (0x02);
#endif
	}
};

namespace FP8Types {

	enum FaderMode {
		ModeTrack,
		ModePlugins,
		ModeSend,
		ModePan
	};

	enum NavigationMode {
		NavChannel,
		NavZoom,
		NavScroll,
		NavBank,
		NavMaster,
		NavSection,
		NavMarker,
		NavPan /* FP2 only */
	};

	enum MixMode {
		MixAudio,
		MixInstrument,
		MixBus,
		MixVCA,
		MixAll,
		MixInputs,
		MixMIDI,
		MixOutputs,
		MixFX,
		MixUser,
		MixModeMax = MixUser
	};

};

} } /* namespace */
#endif /* _ardour_surfaces_fp8base_h_ */
