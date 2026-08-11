/*
 * Copyright (C) 2026 Ada <ada@6bit.com>
 * Copyright (C) 2026 Joshua Perry <josh@6bit.com>
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

/* Wire protocol for the Native Instruments Komplete Kontrol A-Series.
 *
 * Established by observing the device's own HID report descriptor and by
 * capture on an A61 (firmware build 2022-01-05).  No code or data was taken
 * from any GPL-3 source; where a third-party project's published findings
 * agree they are cited as corroboration only.
 *
 * This header is protocol description, deliberately free of Ardour types, so
 * it can be read and checked against a capture on its own.
 */

#ifndef _ardour_surfaces_kka_protocol_h_
#define _ardour_surfaces_kka_protocol_h_

#include <stddef.h>
#include <stdint.h>

namespace ArdourSurface { namespace KKA {

/* ------------------------------------------------------------------ USB */

static const uint16_t VendorID = 0x17cc;

/* The three A-Series keyboards are one device with three keybeds; NI's own
 * manual states "beyond the keybed, all keyboards come with identical
 * features".  Only the A61 has been tested here.  The A49's report descriptor
 * is 502 bytes, matching the A61's exactly; the A25's is unmeasured -- no
 * public capture of one exists -- which is why open_device() checks the
 * descriptor length and warns rather than assuming.
 *
 * The M32 (0x1860) is the same family but NOT the same device: identical
 * button bitfield, LED report and display protocol, but a 526-byte descriptor
 * and a different analog block (touch strips in place of pitch/mod wheels,
 * encoder at payload[23]).  It needs a variant, not just another PID here.
 */
struct Variant {
	uint16_t    pid;
	int         keys;
	const char* model;
	bool        tested;
};

extern const Variant Variants[];
extern const size_t  NumVariants;

const Variant* variant_for_pid (uint16_t pid);

/* --------------------------------------------------------- HID reports */

enum ReportID {
	ReportInput   = 0x01, /* IN,  29 byte payload, sent on change only */
	ReportLEDs    = 0x80, /* OUT, 21 bytes, each 0..127                */
	ReportMode    = 0xa0, /* OUT, 2 bytes                              */
	ReportDisplay = 0xe0, /* OUT, 8 byte header + 256 byte bitmap      */
};

static const size_t InputPayloadSize = 29;
static const size_t InputReportSize  = 1 + InputPayloadSize; /* 30 */

static const size_t LEDCount      = 21;
static const size_t LEDReportSize = 1 + LEDCount; /* 22 */

static const size_t DisplayHeaderSize  = 8;
static const size_t DisplayPayloadSize = 256;
static const size_t DisplayReportSize  = 1 + DisplayHeaderSize + DisplayPayloadSize; /* 265 */

/* Report descriptor length, verified identical on A61 and A49. */
static const size_t ExpectedDescriptorSize = 502;

/* ---------------------------------------------------------------- modes */

/* Payload for ReportMode.  Interactive mode moves knob rotation off the MIDI
 * port and into HID as endless encoders, and hands the display to the host.
 * It does NOT survive a replug, so the surface must re-assert it and must
 * restore MIDI mode on teardown or the user is left with a half-owned device.
 */
static const uint8_t ModeMIDI[2]        = { 0x07, 0x00 };
static const uint8_t ModeInteractive[2] = { 0x03, 0x04 };

/* ------------------------------------------------- input report layout */

/* Offsets are into the payload, i.e. with the report ID already stripped. */
static const size_t ButtonsOffset = 0;  /* 5 bytes, 40 declared bits       */
static const size_t ButtonsBytes  = 5;
static const size_t KnobsOffset   = 5;  /* 8 x uint16 little-endian        */
static const size_t EncoderOffset = 27; /* low nibble only                 */

/* payload[21:27] declare pitch bend, mod wheel and a third analog input.
 * All three read 0 in both modes on the A61 -- they are presumably
 * provisioned for another model in the family.  Do not plan around them.
 * payload[28] is a constant 36 in every capture; purpose unknown.
 */

static const int NumKnobs = 8;

/* Every control's identity is its bit index in the button bitfield, and for
 * the first LEDCount of them that is also its index in the LED report.  There
 * is no mapping table to write in either direction.
 */
enum ControlID {
	Shift = 0,       /* swallowed by the firmware in MIDI mode */
	Scale,
	Arp,
	Undo,
	Quantize,
	Ideas,
	Loop,
	Metro,
	Tempo,
	Play,
	Rec,
	Stop,
	PresetUp,
	PresetDown,
	PageLeft,
	PageRight,
	Browser,
	PlugIn,
	Track,
	OctaveDown,
	OctaveUp,        /* == LEDCount - 1; nothing below here has an LED */
	Encoder4DUp,
	Encoder4DLeft,
	Encoder4DRight,
	Encoder4DDown,
	Knob1Touch,
	Knob2Touch,
	Knob3Touch,
	Knob4Touch,
	Knob5Touch,
	Knob6Touch,
	Knob7Touch,
	Knob8Touch,
	Encoder4DPress,
	NumControls      /* 34; declared bits 34..39 are unused padding */
};

const char* control_name (ControlID);

static inline bool control_has_led (ControlID c)
{
	return (size_t) c < LEDCount;
}

/* LED brightness.  The descriptor declares a full 0..127 range, so this is
 * plausibly a continuum, but only off and full have been exercised.
 */
static const uint8_t LEDOff    = 0x00;
static const uint8_t LEDOn     = 0x7c;
static const uint8_t LEDBright = 0x7e;

/* Deliberately far from the others, because it is also the experiment that
 * answers open question 1 -- whether brightness is a continuum or a handful of
 * quantised steps. LEDOn and LEDBright differ by 2 out of 127 and would look
 * identical whatever the answer, so they cannot tell us. If this reads as
 * visibly dimmer than LEDOn the range is usable for signalling intensity; if
 * it reads as off or as full brightness, it is not.
 */
static const uint8_t LEDDim    = 0x20;

/* ------------------------------------------------------------ encoders */

/* The 4-D encoder is a 4-bit wrapping absolute position, not a delta. */
static inline int encoder_delta (uint8_t prev, uint8_t cur)
{
	return (((int) cur - (int) prev + 8) & 0x0f) - 8;
}

/* Each knob is a wrapping counter modulo KnobRange, moving KnobUnitsPerStep at
 * a time.  The knobs turn freely and have no physical detent, so a "step" here
 * is the device's reporting quantum and not something the user can feel.
 * Reports coalesce during a fast spin, so a single report can carry several
 * steps; never assume one.
 *
 * Both constants are measured on an A61.  Successive reports through a slow
 * turn differ by exactly 8, and turning down through zero reports 991 -- which
 * is 999 - 8, so the modulus is 999 rather than the 1000 one would guess.
 */
static const int KnobRange        = 999;
static const int KnobUnitsPerStep = 8;

static inline int knob_delta (uint16_t prev, uint16_t cur)
{
	int raw = (int) cur - (int) prev;
	if (raw >  KnobRange / 2) { raw -= KnobRange; }
	if (raw < -KnobRange / 2) { raw += KnobRange; }
	return raw;
}

/* ------------------------------------------------------------- display */

/* 128x32 monochrome, read from feature report 0xf8.
 *
 * The header is four uint16 little-endian fields, and the trap is that they
 * are in mixed units: x and width are pixels, y and height are 8-row pages.
 * The firmware validates the header and silently drops anything off-panel, so
 * a wrong guess is indistinguishable from a dead device.
 *
 * Payload is SSD1306-style page-major -- data[page * width + col], each byte
 * eight vertical pixels with bit 0 the TOP row -- and the polarity is
 * INVERTED: a set bit renders dark.  So an all-zero payload lights the whole
 * panel and an all-ones payload blanks it.
 *
 * Short reports do not work: the device appears to consume 256 payload bytes
 * regardless of what was sent, so every report costs a fixed 265 bytes no
 * matter how small the rectangle in its header.
 */
static const int DisplayWidth  = 128;
static const int DisplayHeight = 32;
static const int DisplayPages  = DisplayHeight / 8; /* 4 */

} } /* namespace ArdourSurface::KKA */

#endif /* _ardour_surfaces_kka_protocol_h_ */
