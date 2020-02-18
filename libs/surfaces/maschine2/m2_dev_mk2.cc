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
#include "m2_dev_mk2.h"

#include <pangomm/fontdescription.h>

#include "images.h"

static size_t maschine_png_readoff = 0;

static Cairo::ErrorStatus maschine_png_read (unsigned char* d, unsigned int s) {
	if (s + maschine_png_readoff > sizeof (maschine_png)) {
		return CAIRO_STATUS_READ_ERROR;
	}
	memcpy (d, &maschine_png[maschine_png_readoff], s);
	maschine_png_readoff += s;
	return CAIRO_STATUS_SUCCESS;
}

using namespace ArdourSurface;

Maschine2Mk2::Maschine2Mk2 () : M2Device ()
{
	_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, 512, 64);
	clear (true);
}

void
Maschine2Mk2::clear (bool splash)
{
	M2Device::clear (splash);

	memset (&ctrl_in, 0, sizeof (ctrl_in));
	memset (pad, 0, sizeof (pad));

	ctrl80[0] = 0xff;
	ctrl81[0] = 0xff;
	ctrl82[0] = 0xff;

	for (int d = 0; d < 2; ++d) {
		for (int l = 0; l < 8; ++l) {
			_img[d][l][0] = 0xff;
		}
	}

#if 0
	Cairo::RefPtr<Cairo::Context> c = Cairo::Context::create (_surface);
	c->set_operator (Cairo::OPERATOR_CLEAR);
	c->paint ();
	return;
#endif

	maschine_png_readoff = 0;
	Cairo::RefPtr<Cairo::ImageSurface> sf = Cairo::ImageSurface::create_from_png_stream (sigc::ptr_fun (maschine_png_read));
	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (_surface);
	cr->set_source(sf, 0, 0);
	cr->paint ();

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (cr);
	Pango::FontDescription fd ("Sans Bold 18px");
	layout->set_font_description (fd);
	layout->set_alignment (Pango::ALIGN_CENTER);

	int cx;
	if (splash) {
		layout->set_text (string_compose ("%1\n%2", PROGRAM_NAME, VERSIONSTRING));
		cx = 384;
	} else {
		cr->rectangle (326, 0, 186, 64);
		cr->set_source_rgb (0, 0, 0);
		cr->fill ();
		layout->set_text ("Keep Groovin'");
		cx = 421;
	}

	int tw, th;
	layout->get_pixel_size (tw, th);
	cr->move_to (cx - tw * 0.5, 32 - th * 0.5);
	cr->set_source_rgb (1, 1, 1);
	layout->show_in_cairo_context(cr);
	//_surface->write_to_png ("/tmp/amaschine.png");
}

void
Maschine2Mk2::read (hid_device* handle, M2Contols* ctrl)
{
	assert (ctrl);
	while (true) {
		uint8_t buf[256];
		int res = hid_read (handle, buf, 256);
		if (res < 1) {
			return;
		}

		// TODO parse incrementally if chunked at 64

		if (res > 24 && buf[0] == 0x01) {
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
Maschine2Mk2::write (hid_device* handle, M2Contols* ctrl)
{
	bump_blink ();
	uint8_t buf[265];

	//TODO double-buffer, send changes only if needed

	/* 31 control buttons: 8 mst + 8 top + 8 pads + 7 mst
	 * 8-bit brightness
	 */
	buf[0] = 0x82;
	set_colors82 (ctrl, &buf[1]);
	if (memcmp (ctrl82, buf, 32)) {
			hid_write (handle, buf, 32);
			memcpy (ctrl82, buf, 32);
	}

	/* 8 group rgb|rgb + 8 on/off transport buttons */
	buf[0] = 0x81;
	set_colors81 (ctrl, &buf[1]);
	if (memcmp (ctrl81, buf, 57)) {
			hid_write (handle, buf, 57);
			memcpy (ctrl81, buf, 57);
	}

	/* 16 RGB grid pads */
	buf[0] = 0x80;
	set_colors80 (ctrl, &buf[1]);
	if (memcmp (ctrl80, buf, 49)) {
			hid_write (handle, buf, 49);
			memcpy (ctrl80, buf, 49);
	}

	if (_splashcnt < _splashtime) {
		++_splashcnt;
	}
	else if (! vblank () /* EMIT SIGNAL*/) {
		/* check clear/initial draw */
		if (_img[0][0][0] != 0xff) {
			return;
		}
	}

	/* display */
	_surface->flush ();
	const unsigned char* img = _surface->get_data ();
	const int stride = _surface->get_stride ();
	for (int d = 0; d < 2; ++d) {
		memset (buf, 0, 9);
		buf[0] = 0xe0 | d;
		for (int l = 0; l < 8; ++l) {
			buf[3] = 8 * l;
			buf[5] = 0x20;
			buf[7] = 0x08;

			int y0 = l * 8;
			int x0 = d * 256;

			for (int p = 0; p < 256; ++p) {
				uint8_t v = 0;
				const int y = y0 + p / 32;
				for (int b = 0; b < 8; ++b) {
					const int x = x0 + (p % 32) * 8 + b;
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
			if (memcmp (_img[d][l], buf, 265)) {
				hid_write (handle, buf, 265);
				memcpy (_img[d][l], buf, 265);
			}
		}
	}
}

void
Maschine2Mk2::assign_controls (M2Contols* ctrl) const
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

	ASSIGN (BtnScene,     pads_scene);
	ASSIGN (BtnPattern,   pads_pattern);
	ASSIGN (BtnPadMode,   pads_mode);
	ASSIGN (BtnNavigate,  pads_navigate);
	ASSIGN (BtnDuplicate, pads_duplicate);
	ASSIGN (BtnSelect,    pads_select);
	ASSIGN (BtnSolo,      pads_solo);
	ASSIGN (BtnMute,      pads_mute);

	ASSIGN (BtnControl,  top_control);
	ASSIGN (BtnStep,     top_step);
	ASSIGN (BtnBrowse,   top_browse);
	ASSIGN (BtnSampling, top_sampling);
	ASSIGN (BtnSelLeft,  top_left);
	ASSIGN (BtnSelRight, top_right);
	ASSIGN (BtnAll,      top_all);
	ASSIGN (BtnAuto,     top_auto);

	ASSIGN (BtnVolume,     mst_volume);
	ASSIGN (BtnSwing,      mst_swing);
	ASSIGN (BtnTempo,      mst_tempo);
	ASSIGN (BtnNavLeft,    mst_left);
	ASSIGN (BtnNavRight,   mst_right);
	ASSIGN (BtnEnter,      mst_enter);
	ASSIGN (BtnNoteRepeat, mst_note_repeat);
	ASSIGN (BtnWheel,      mst_wheel);

	ASSIGN (BtnGroupA, groups_a);
	ASSIGN (BtnGroupB, groups_b);
	ASSIGN (BtnGroupC, groups_c);
	ASSIGN (BtnGroupD, groups_d);
	ASSIGN (BtnGroupE, groups_e);
	ASSIGN (BtnGroupF, groups_f);
	ASSIGN (BtnGroupG, groups_g);
	ASSIGN (BtnGroupH, groups_h);

	ASSIGN (BtnTop0, top_0);
	ASSIGN (BtnTop1, top_1);
	ASSIGN (BtnTop2, top_2);
	ASSIGN (BtnTop3, top_3);
	ASSIGN (BtnTop4, top_4);
	ASSIGN (BtnTop5, top_5);
	ASSIGN (BtnTop6, top_6);
	ASSIGN (BtnTop7, top_7);
#undef ASSIGN

	change |= ctrl->encoder (0)->set_value (ctrl_in.mst_wheel_pos);
	for (int i = 0; i < 8; ++i) {
		change |= ctrl->encoder (1 + i)->set_value (ctrl_in.top_knobs[i]);
	}

	if (change && mod == M2Contols::ModShift) {
		M2ToggleHoldButton* btn = dynamic_cast<M2ToggleHoldButton*> (ctrl->button (M2Contols::BtnShift, M2Contols::ModNone));
		if (btn) {
			btn->unset_active_on_release ();
		}
	}
}

#define LIGHT(BIT, BTN) \
	b[BIT] = ctrl->button (M2Contols:: BTN, mod)->lightness (_blink_shade)

#define COLOR(BIT, BTN) \
{ \
	const uint32_t rgb = ctrl->button (M2Contols:: BTN, mod)->color (_blink_shade); \
		b[0 + BIT ] = (rgb >>  0) & 0xff; \
		b[1 + BIT ] = (rgb >>  8) & 0xff; \
		b[2 + BIT ] = (rgb >> 16) & 0xff; \
		b[3 + BIT ] = (rgb >>  0) & 0xff; \
		b[4 + BIT ] = (rgb >>  8) & 0xff; \
		b[5 + BIT ] = (rgb >> 16) & 0xff; \
}

void
Maschine2Mk2::set_colors80 (M2Contols* ctrl, uint8_t* b) const
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
Maschine2Mk2::set_colors81 (M2Contols* ctrl, uint8_t* b) const
{
	if (!ctrl) {
		memset (b, 0, 56);
		return;
	}
	M2Contols::Modifier mod = ctrl->button (M2Contols::BtnShift, M2Contols::ModNone)->active () ? M2Contols::ModShift : M2Contols::ModNone;

	COLOR ( 0, BtnGroupA);
	COLOR ( 6, BtnGroupB);
	COLOR (12, BtnGroupC);
	COLOR (18, BtnGroupD);
	COLOR (24, BtnGroupE);
	COLOR (30, BtnGroupF);
	COLOR (36, BtnGroupG);
	COLOR (42, BtnGroupH);

	LIGHT (48, BtnRestart);
	LIGHT (49, BtnStepLeft);
	LIGHT (50, BtnStepRight);
	LIGHT (51, BtnGrid);
	LIGHT (52, BtnPlay);
	LIGHT (53, BtnRec);
	LIGHT (54, BtnErase);
	LIGHT (55, BtnShift);
}

void
Maschine2Mk2::set_colors82 (M2Contols* ctrl, uint8_t* b) const
{
	if (!ctrl) {
		memset (b, 0, 31);
		return;
	}
	M2Contols::Modifier mod = ctrl->button (M2Contols::BtnShift, M2Contols::ModNone)->active () ? M2Contols::ModShift : M2Contols::ModNone;

	LIGHT ( 0, BtnControl);
	LIGHT ( 1, BtnStep);
	LIGHT ( 2, BtnBrowse);
	LIGHT ( 3, BtnSampling);
	LIGHT ( 4, BtnSelLeft);
	LIGHT ( 5, BtnSelRight);
	LIGHT ( 6, BtnAll);
	LIGHT ( 7, BtnAuto);

	LIGHT ( 8, BtnTop0);
	LIGHT ( 9, BtnTop1);
	LIGHT (10, BtnTop2);
	LIGHT (11, BtnTop3);
	LIGHT (12, BtnTop4);
	LIGHT (13, BtnTop5);
	LIGHT (14, BtnTop6);
	LIGHT (15, BtnTop7);

	LIGHT (16, BtnScene);
	LIGHT (17, BtnPattern);
	LIGHT (18, BtnPadMode);
	LIGHT (19, BtnNavigate);
	LIGHT (20, BtnDuplicate);
	LIGHT (21, BtnSelect);
	LIGHT (22, BtnSolo);
	LIGHT (23, BtnMute);

	LIGHT (24, BtnVolume);
	LIGHT (25, BtnSwing);
	LIGHT (26, BtnTempo);
	LIGHT (27, BtnNavLeft);
	LIGHT (28, BtnNavRight);
	LIGHT (29, BtnEnter);
	LIGHT (30, BtnNoteRepeat);
}
