/*
 * Copyright (C) 2016-2021 Robin Gareus <robin@gareus.org>
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

#include <cairomm/surface.h>
#include <pangomm.h>

#include "ardour/export_analysis.h"
#include "ardour/types.h"

namespace ArdourGraphs {

Cairo::RefPtr<Cairo::ImageSurface>
draw_waveform (Glib::RefPtr<Pango::Context>, ARDOUR::ExportAnalysisPtr, uint32_t channel, int height_2, int margin_left, bool logscale, bool rectified);

Cairo::RefPtr<Cairo::ImageSurface>
draw_spectrum (Glib::RefPtr<Pango::Context>, ARDOUR::ExportAnalysisPtr, int height, int width);

Cairo::RefPtr<Cairo::ImageSurface>
spectrum_legend (Glib::RefPtr<Pango::Context>, int height, int margin_right);

Cairo::RefPtr<Cairo::ImageSurface>
loudness_histogram (Glib::RefPtr<Pango::Context>, ARDOUR::ExportAnalysisPtr, int height, int width = 540);

Cairo::RefPtr<Cairo::ImageSurface>
time_axis (Glib::RefPtr<Pango::Context>, int width, int margin_left, ARDOUR::samplepos_t start, ARDOUR::samplecnt_t len, ARDOUR::samplecnt_t rate);

Cairo::RefPtr<Cairo::ImageSurface>
plot_loudness (Glib::RefPtr<Pango::Context>, ARDOUR::ExportAnalysisPtr, int height, int margin_left, ARDOUR::samplecnt_t rate);

}; // namespace ArdourGraphs
