/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystem.com>
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

#include "ardour/audioregion.h"

#include "waveview/wave_view.h"

#include "audio_clip_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWaveView;
using namespace ArdourCanvas;

AudioClipEditor::AudioClipEditor ()
{

}

AudioClipEditor::~AudioClipEditor ()
{
	drop_waves ();
}

void
AudioClipEditor::drop_waves ()
{
	for (auto & wave : waves) {
		delete wave;
	}

	waves.clear ();
}

void
AudioClipEditor::set_region (boost::shared_ptr<AudioRegion> r)
{
	drop_waves ();

	uint32_t n_chans = r->n_channels ();

	for (uint32_t n = 0; n < n_chans; ++n) {
		WaveView* wv = new WaveView (this, r);
		wv->set_channel (n);
		waves.push_back (wv);
	}

	int h = get_allocation().get_height ();

	if (h) {
		set_wave_heights (h);
	}
}

void
AudioClipEditor::size_allocate (Gtk::Allocation const & alloc)
{
	GtkCanvas::size_allocate (alloc);

	set_wave_heights (alloc.get_height());
}

void
AudioClipEditor::set_wave_heights (int h)
{
	uint32_t n = 0;
	Distance ht = h / waves.size();

	for (auto & wave : waves) {
		wave->set_height (ht);
		wave->set_y_position (n * ht);
		wave->set_samples_per_pixel (8192);
		wave->set_show_zero_line (false);
		wave->set_clip_level (1.0);
	}
}

void
AudioClipEditor::set_waveform_colors ()
{
	Gtkmm2ext::Color clip = UIConfiguration::instance().color ("clipped waveform");
	Gtkmm2ext::Color zero = UIConfiguration::instance().color ("zero line");
	Gtkmm2ext::Color fill = UIConfiguration::instance().color ("waveform fill");
	Gtkmm2ext::Color outline = UIConfiguration::instance().color ("waveform outline");

	for (auto & wave : waves) {
		wave->set_fill_color (fill);
		wave->set_outline_color (outline);
		wave->set_clip_color (clip);
		wave->set_zero_color (zero);
	}
}

