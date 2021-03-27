/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#include "canvas/arrow.h"
#include "canvas/tracking_text.h"

#include "ardour/audioregion.h"
#include "ardour/dB.h"

#include "audio_region_view.h"
#include "region_peak_cursor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;

RegionPeakCursor::RegionPeakCursor (ArdourCanvas::Item* parent)
{
	_canvas_text = new ArdourCanvas::Text (parent);
	_canvas_text->set_outline (true);
	_canvas_text->set_font_description (Pango::FontDescription (UIConfiguration::instance ().get_NormalMonospaceFont ()));
	_canvas_text->set_ignore_events (true);

	_canvas_line = new ArdourCanvas::Arrow (parent);
	_canvas_line->set_show_head (0, true);
	_canvas_line->set_show_head (1, true);
	_canvas_line->set_ignore_events (true);

	color_handler ();
	UIConfiguration::instance ().ColorsChanged.connect (sigc::mem_fun (*this, &RegionPeakCursor::color_handler));
}

RegionPeakCursor::~RegionPeakCursor ()
{
#if 0
	_canvas_text->unparent ();
	delete _canvas_text;
	_canvas_line->unparent ();
	delete _canvas_line;
#endif
}

void
RegionPeakCursor::color_handler ()
{
	_canvas_text->set_color (UIConfiguration::instance ().color ("gtk_foreground"));
}

void
RegionPeakCursor::show ()
{
	_canvas_line->show ();
	_canvas_line->raise_to_top ();
	_canvas_text->show ();
	_canvas_text->raise_to_top ();
	_canvas_text->parent ()->raise_to_top ();
}

void
RegionPeakCursor::hide ()
{
	_canvas_text->hide ();
	_canvas_line->hide ();
}

void
RegionPeakCursor::set (AudioRegionView* arv, samplepos_t when, samplecnt_t samples_per_pixel)
{
	boost::shared_ptr<ARDOUR::AudioRegion> ar = boost::dynamic_pointer_cast<ARDOUR::AudioRegion> (arv->region ());
	assert (ar);
	assert (ar->n_channels () > 0);

	sampleoffset_t s = when - ar->position_sample ();
	if (s < 0 || s > ar->length_samples ()) {
		hide ();
		return;
	}

	/* read_peaks() offset is relative to the region's source */
	s += ar->start_sample ();

	PeakData p;
	for (uint32_t chn = 0; chn < ar->n_channels (); ++chn) {
		PeakData pc;
		ar->read_peaks (&pc, 1, s, samples_per_pixel, chn, samples_per_pixel);
		if (chn == 0) {
			p.min = pc.min;
			p.max = pc.max;
		} else {
			p.min = std::min (p.min, pc.min);
			p.max = std::max (p.max, pc.max);
		}
	}

	char tmp[128];
	sprintf (tmp, "%s %+.2f %5.1f %s\n%s %+.2f %5.1f %s\n",
	         _("Max:"), p.max,
	         accurate_coefficient_to_dB (fabsf (p.max)), _("dBFS"),
	         _("Min:"), p.min,
	         accurate_coefficient_to_dB (fabsf (p.min)), _("dBFS"));

	_canvas_text->set (tmp);

	/* position relative to editor origin */
	ArdourCanvas::Duple pos  = arv->get_canvas_group ()->item_to_window (_canvas_text->parent ()->position ());
	double              xpos = pos.x + floor ((double)(when - ar->position_sample ()) / samples_per_pixel);

	_canvas_text->set_x_position (xpos + 3);
	_canvas_text->set_y_position (pos.y + 3);

	_canvas_line->set_x (xpos - 0.5);
	_canvas_line->set_y0 (pos.y);
	_canvas_line->set_y1 (pos.y + arv->height ());

	if (!visible ()) {
		show ();
	}
}

bool
RegionPeakCursor::visible () const
{
	return _canvas_text->visible ();
}
