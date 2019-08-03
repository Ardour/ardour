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

#include <math.h>

#include "pbd/compose.h"

#include "maschine2.h"
#include "m2controls.h"
#include "m2_dev_mikro.h"

#include <pangomm/fontdescription.h>

#include "images.h"

static size_t mikro_png_readoff = 0;

static Cairo::ErrorStatus maschine_png_read (unsigned char* d, unsigned int s) {
	if (s + mikro_png_readoff > sizeof (mikro_png)) {
		return CAIRO_STATUS_READ_ERROR;
	}
	memcpy (d, &mikro_png[mikro_png_readoff], s);
	mikro_png_readoff += s;
	return CAIRO_STATUS_SUCCESS;
}

using namespace ArdourSurface;

Maschine2Mikro::Maschine2Mikro () : M2Device ()
{
	_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, 128, 64);
	clear (true);
}

void
Maschine2Mikro::clear (bool splash)
{
	M2Device::clear (splash);

	memset (&ctrl_in, 0, sizeof (ctrl_in));
	memset (pad, 0, sizeof (pad));

	_lights[0] = 0xff;

	for (int l = 0; l < 4; ++l) {
		_img[l][0] = 0xff;
	}

	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (_surface);
	if (!splash) {
		mikro_png_readoff = 0;
		Cairo::RefPtr<Cairo::ImageSurface> sf = Cairo::ImageSurface::create_from_png_stream (sigc::ptr_fun (maschine_png_read));
		cr->set_source(sf, 0, 0);
		cr->paint ();
	} else {
		cr->set_operator (Cairo::OPERATOR_CLEAR);
		cr->paint ();
		cr->set_operator (Cairo::OPERATOR_OVER);

		Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (cr);
		Pango::FontDescription fd ("Sans Bold 18px");
		layout->set_font_description (fd);
		layout->set_alignment (Pango::ALIGN_CENTER);

		layout->set_text (string_compose ("%1\n%2", PROGRAM_NAME, VERSIONSTRING));
		int tw, th;
		layout->get_pixel_size (tw, th);
		cr->move_to (128 - tw * 0.5, 32 - th * 0.5);
		cr->set_source_rgb (1, 1, 1);
		layout->show_in_cairo_context(cr);
	}
	//_surface->write_to_png ("/tmp/amaschine.png");
}

void
Maschine2Mikro::read (hid_device* handle, M2Contols* ctrl)
{
	assert (ctrl);
	while (true) {
		uint8_t buf[256];
		int res = hid_read (handle, buf, 256);
		if (res < 1) {
			return;
		}

		// TODO parse incrementally if chunked at 64

		if (res > 4 && buf[0] == 0x01) {
			memcpy (&ctrl_in, &buf[1], sizeof (ctrl_in));
			assign_controls (ctrl);
		}
		else if (res > 32 && buf[0] == 0x20) {
			for (unsigned int i = 0; i < 16; ++i) {
				uint8_t v0 = buf[1 + 2 * i];
				uint8_t v1 = buf[2 + 2 * i];
				uint8_t p = (v1 & 0xf0) >> 4;
				pad[p] = ((v1 & 0xf) << 8) | v0;
				unsigned int pid = 15 - ((i & 0xc) + (3 - (i & 0x3)));
				ctrl->pad (pid)->set_value (pad[p]);
			}
			// TODO read complete 65 byte msg, expect buf[33] == 0x00
		}
	}
}

void
Maschine2Mikro::write (hid_device* handle, M2Contols* ctrl)
{
	bump_blink ();
	uint8_t buf[265];

	//TODO double-buffer, send changes only if needed

	/* 30 control buttons, 8-bit brightness,
	 * + 16 RGB pads
	 */
	buf[0] = 0x80;
	set_lights (ctrl, &buf[1]);
	set_pads (ctrl, &buf[31]);
	if (memcmp (_lights, buf, 79)) {
			hid_write (handle, buf, 79);
			memcpy (_lights, buf, 79);
	}

	if (_splashcnt < _splashtime ) {
		++_splashcnt;
	}
	else if (! vblank () /* EMIT SIGNAL*/) {
		/* check clear/initial draw */
		if (_img[0][0] != 0xff) {
			return;
		}
	}

	/* display */
	_surface->flush ();
	const unsigned char* img = _surface->get_data ();
	const int stride = _surface->get_stride ();
	memset (buf, 0, 9);
	buf[0] = 0xe0;
	for (int l = 0; l < 4; ++l) {
		buf[1] = 32 * l;
		buf[5] = 0x20;
		buf[7] = 0x08;

		int y0 = l * 16;
		for (int p = 0; p < 256; ++p) {
			uint8_t v = 0;
			const int y = y0 + p / 16;
			for (int b = 0; b < 8; ++b) {
				const int x = (p % 16) * 8 + b;
				int off = y * stride + x * 4 /* ARGB32 */;
				/* off + 0 == blue
				 * off + 1 == green
				 * off + 2 == red
				 * off + 3 == alpha
				 */
				/* calculate lightness */
				uint8_t l = std::max (img[off + 0], std::max (img[off + 1], img[off + 2]));
				if (l > 0x7e) { // TODO: take alpha channel into account?!
					v |= 1 << (7 - b);
				}
			}
			buf[9 + p] = v;
		}
		if (memcmp (_img[l], buf, 265)) {
			hid_write (handle, buf, 265);
			memcpy (_img[l], buf, 265);
		}
	}
}

void
Maschine2Mikro::assign_controls (M2Contols* ctrl) const
{
	ctrl->button (M2Contols::BtnShift, M2Contols::ModNone)->set_active (ctrl_in.trs_shift ? true : false);
	M2Contols::Modifier mod = ctrl->button (M2Contols::BtnShift, M2Contols::ModNone)->active () ? M2Contols::ModShift : M2Contols::ModNone;

	bool change = false;
#define ASSIGN(BTN, VAR) \
	change |= ctrl->button (M2Contols:: BTN, mod)->set_active (ctrl_in. VAR ? true : false)

	ASSIGN (BtnRestart,   trs_restart);
	ASSIGN (BtnStepLeft,  trs_left);
	ASSIGN (BtnStepRight, trs_right);
	ASSIGN (BtnGrid,      trs_grid);
	ASSIGN (BtnPlay,      trs_play);
	ASSIGN (BtnRec,       trs_rec);
	ASSIGN (BtnErase,     trs_erase);

	ASSIGN (BtnGroupA,     group);
	ASSIGN (BtnBrowse,     browse);
	ASSIGN (BtnSampling,   sampling);
	ASSIGN (BtnNoteRepeat, note_repeat);
	ASSIGN (BtnWheel,      mst_wheel);

	ASSIGN (BtnTop0, f1);
	ASSIGN (BtnTop1, f1);
	ASSIGN (BtnTop2, f3);

	ASSIGN (BtnControl,    control);
	ASSIGN (BtnNavigate,   navigate); // XXX
	ASSIGN (BtnNavLeft,    nav_left);
	ASSIGN (BtnNavRight,   nav_right);
	ASSIGN (BtnEnter,      main);

	ASSIGN (BtnScene,     pads_scene);
	ASSIGN (BtnPattern,   pads_pattern);
	ASSIGN (BtnPadMode,   pads_mode);
	ASSIGN (BtnNavigate,  pads_navigate);
	ASSIGN (BtnDuplicate, pads_duplicate);
	ASSIGN (BtnSelect,    pads_select);
	ASSIGN (BtnSolo,      pads_solo);
	ASSIGN (BtnMute,      pads_mute);
#undef ASSIGN

	change |= ctrl->encoder (0)->set_value (ctrl_in.mst_wheel_pos);

	if (change && mod == M2Contols::ModShift) {
		M2ToggleHoldButton* btn = dynamic_cast<M2ToggleHoldButton*> (ctrl->button (M2Contols::BtnShift, M2Contols::ModNone));
		if (btn) {
			btn->unset_active_on_release ();
		}
	}
}

#define LIGHT(BIT, BTN) \
	b[BIT] = ctrl->button (M2Contols:: BTN, mod)->lightness (_blink_shade)

void
Maschine2Mikro::set_pads (M2Contols* ctrl, uint8_t* b) const
{
	if (!ctrl) {
		memset (b, 0, 48);
		return;
	}
	for (unsigned int i = 0; i < 16; ++i) {
		unsigned int pid = 15 - ((i & 0xc) + (3 - (i & 0x3)));
		ctrl->pad (pid)->color (b[i * 3], b[1 + i * 3], b[2 + i * 3]);
	}
}

void
Maschine2Mikro::set_lights (M2Contols* ctrl, uint8_t* b) const
{
	if (!ctrl) {
		memset (b, 0, 29);
		return;
	}
	M2Contols::Modifier mod = ctrl->button (M2Contols::BtnShift, M2Contols::ModNone)->active () ? M2Contols::ModShift : M2Contols::ModNone;

	LIGHT ( 0, BtnTop0); // F1
	LIGHT ( 1, BtnTop1); // F2
	LIGHT ( 2, BtnTop2); // F3
	LIGHT ( 3, BtnControl);
	LIGHT ( 4, BtnNavigate); // XXX
	LIGHT ( 5, BtnNavLeft);
	LIGHT ( 6, BtnNavRight);
	LIGHT ( 7, BtnEnter); // Main

	const uint32_t rgb = ctrl->button (M2Contols::BtnGroupA, mod)->color (_blink_shade);
	b[8]  = (rgb >>  0) & 0xff;
	b[9]  = (rgb >>  8) & 0xff;
	b[10] = (rgb >> 16) & 0xff;

	LIGHT (11, BtnBrowse);
	LIGHT (12, BtnSampling);
	LIGHT (13, BtnNoteRepeat);

	LIGHT (14, BtnRestart);
	LIGHT (15, BtnStepLeft);
	LIGHT (16, BtnStepRight);
	LIGHT (17, BtnGrid);
	LIGHT (18, BtnPlay);
	LIGHT (19, BtnRec);
	LIGHT (20, BtnErase);
	LIGHT (21, BtnShift);

	LIGHT (22, BtnScene);
	LIGHT (23, BtnPattern);
	LIGHT (24, BtnPadMode);
	LIGHT (25, BtnNavigate);
	LIGHT (26, BtnDuplicate);
	LIGHT (27, BtnSelect);
	LIGHT (28, BtnSolo);
	LIGHT (29, BtnMute);
}
