/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <cmath>
#include <cassert>
#include <algorithm>
#include <vector>

#include <boost/scoped_array.hpp>

#include <gtkmm.h>

#include "ardour/playlist.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "pbd/memento_command.h"

#include "evoral/Curve.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/colors.h"

#include "canvas/rectangle.h"
#include "canvas/polygon.h"
#include "canvas/poly_line.h"
#include "canvas/line.h"
#include "canvas/text.h"
#include "canvas/xfade_curve.h"
#include "canvas/debug.h"

#include "waveview/debug.h"

#include "streamview.h"
#include "audio_region_view.h"
#include "audio_time_axis.h"
#include "enums_convert.h"
#include "public_editor.h"
#include "audio_region_editor.h"
#include "audio_streamview.h"
#include "region_gain_line.h"
#include "control_point.h"
#include "ghostregion.h"
#include "audio_time_axis.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

static double const handle_size = 10; /* height of fade handles */

Cairo::RefPtr<Cairo::Pattern> AudioRegionView::pending_peak_pattern;

static Cairo::RefPtr<Cairo::Pattern> create_pending_peak_pattern() {
	cairo_surface_t * is = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);

	// create checker pattern
	unsigned char *img = cairo_image_surface_get_data (is);
	cairo_surface_flush (is);
	const int stride = cairo_image_surface_get_stride (is);

	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			const int off = (y * stride + x * 4);
			uint32_t *pixel = (uint32_t*) &img[off];
			if ((x < 4) ^ (y < 4)) {
				*pixel = 0xa0000000;
			} else {
				*pixel = 0x40000000;
			}
		}
	}
	cairo_surface_mark_dirty (is);

	cairo_pattern_t* pat = cairo_pattern_create_for_surface (is);
	cairo_pattern_set_extend (pat, CAIRO_EXTEND_REPEAT);
	Cairo::RefPtr<Cairo::Pattern> p (new Cairo::Pattern (pat, false));
	cairo_surface_destroy (is);
	return p;
}

AudioRegionView::AudioRegionView (ArdourCanvas::Container *parent, RouteTimeAxisView &tv, boost::shared_ptr<AudioRegion> r, double spu,
				  uint32_t basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, sync_mark(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, fade_in_trim_handle(0)
	, fade_out_trim_handle(0)
	, pending_peak_data(0)
	, start_xfade_curve (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_curve (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis(1.0)
	, trim_fade_in_drag_active(false)
	, trim_fade_out_drag_active(false)
{
}

AudioRegionView::AudioRegionView (ArdourCanvas::Container *parent, RouteTimeAxisView &tv, boost::shared_ptr<AudioRegion> r, double spu,
				  uint32_t basic_color, bool recording, TimeAxisViewItem::Visibility visibility)
	: RegionView (parent, tv, r, spu, basic_color, recording, visibility)
	, sync_mark(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, fade_in_trim_handle(0)
	, fade_out_trim_handle(0)
	, pending_peak_data(0)
	, start_xfade_curve (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_curve (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis(1.0)
	, trim_fade_in_drag_active(false)
	, trim_fade_out_drag_active(false)
{
}

AudioRegionView::AudioRegionView (const AudioRegionView& other, boost::shared_ptr<AudioRegion> other_region)
	: RegionView (other, boost::shared_ptr<Region> (other_region))
	, fade_in_handle(0)
	, fade_out_handle(0)
	, fade_in_trim_handle(0)
	, fade_out_trim_handle(0)
	, pending_peak_data(0)
	, start_xfade_curve (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_curve (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis (other._amplitude_above_axis)
	, trim_fade_in_drag_active(false)
	, trim_fade_out_drag_active(false)
{
	init (true);
}

void
AudioRegionView::init (bool wfd)
{
	// FIXME: Some redundancy here with RegionView::init.  Need to figure out
	// where order is important and where it isn't...

	if (!pending_peak_pattern) {
		pending_peak_pattern = create_pending_peak_pattern();
	}

	// needs to be created first, RegionView::init() calls set_height()
	pending_peak_data = new ArdourCanvas::Rectangle (group);
	CANVAS_DEBUG_NAME (pending_peak_data, string_compose ("pending peak rectangle for %1", region()->name()));
	pending_peak_data->set_outline_color (Gtkmm2ext::rgba_to_color (0, 0, 0, 0.0));
	pending_peak_data->set_pattern (pending_peak_pattern);
	pending_peak_data->set_data ("regionview", this);
	pending_peak_data->hide ();

	RegionView::init (wfd);

	_amplitude_above_axis = 1.0;

	create_waves ();

	if (!_recregion) {
		fade_in_handle = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (fade_in_handle, string_compose ("fade in handle for %1", region()->name()));
		fade_in_handle->set_outline_color (Gtkmm2ext::rgba_to_color (0, 0, 0, 1.0));
		fade_in_handle->set_fill_color (UIConfiguration::instance().color ("inactive fade handle"));
		fade_in_handle->set_data ("regionview", this);
		fade_in_handle->hide ();

		fade_out_handle = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (fade_out_handle, string_compose ("fade out handle for %1", region()->name()));
		fade_out_handle->set_outline_color (Gtkmm2ext::rgba_to_color (0, 0, 0, 1.0));
		fade_out_handle->set_fill_color (UIConfiguration::instance().color ("inactive fade handle"));
		fade_out_handle->set_data ("regionview", this);
		fade_out_handle->hide ();

		fade_in_trim_handle = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (fade_in_handle, string_compose ("fade in trim handle for %1", region()->name()));
		fade_in_trim_handle->set_outline_color (Gtkmm2ext::rgba_to_color (0, 0, 0, 1.0));
		fade_in_trim_handle->set_fill_color (UIConfiguration::instance().color ("inactive fade handle"));
		fade_in_trim_handle->set_data ("regionview", this);
		fade_in_trim_handle->hide ();

		fade_out_trim_handle = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (fade_out_handle, string_compose ("fade out trim handle for %1", region()->name()));
		fade_out_trim_handle->set_outline_color (Gtkmm2ext::rgba_to_color (0, 0, 0, 1.0));
		fade_out_trim_handle->set_fill_color (UIConfiguration::instance().color ("inactive fade handle"));
		fade_out_trim_handle->set_data ("regionview", this);
		fade_out_trim_handle->hide ();
	}

	setup_fade_handle_positions ();

	if (!trackview.session()->config.get_show_region_fades()) {
		set_fade_visibility (false);
	}

	const string line_name = _region->name() + ":gain";

	gain_line.reset (new AudioRegionGainLine (line_name, *this, *group, audio_region()->envelope()));

	update_envelope_visibility ();
	gain_line->reset ();

	/* streamview will call set_height() */
	//set_height (trackview.current_height()); // XXX not correct for Layered mode, but set_height() will fix later.

	region_muted ();
	region_sync_changed ();

	region_resized (ARDOUR::bounds_change);
	/* region_resized sets ghost region duration */

	/* region_locked is a synonym for region_renamed () which is called in region_muted() above */
	//region_locked ();

	envelope_active_changed ();
	fade_in_active_changed ();
	fade_out_active_changed ();

	reset_width_dependent_items (_pixel_width);

	if (fade_in_handle) {
		fade_in_handle->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_in_handle_event), fade_in_handle, this, false));
	}

	if (fade_out_handle) {
		fade_out_handle->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_out_handle_event), fade_out_handle, this, false));
	}

	if (fade_in_trim_handle) {
		fade_in_trim_handle->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_in_handle_event), fade_in_trim_handle, this, true));
	}

	if (fade_out_trim_handle) {
		fade_out_trim_handle->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_out_handle_event), fade_out_trim_handle, this, true));
	}

	set_colors ();

	setup_waveform_visibility ();

	/* XXX sync mark drag? */
}

AudioRegionView::~AudioRegionView ()
{
	in_destructor = true;

	RegionViewGoingAway (this); /* EMIT_SIGNAL */

	for (vector<ScopedConnection*>::iterator i = _data_ready_connections.begin(); i != _data_ready_connections.end(); ++i) {
		delete *i;
	}
	_data_ready_connections.clear ();

	for (list<std::pair<samplepos_t, ArdourCanvas::Line*> >::iterator i = feature_lines.begin(); i != feature_lines.end(); ++i) {
		delete ((*i).second);
	}

	/* all waveviews etc will be destroyed when the group is destroyed */
}

boost::shared_ptr<ARDOUR::AudioRegion>
AudioRegionView::audio_region() const
{
	// "Guaranteed" to succeed...
	return boost::dynamic_pointer_cast<AudioRegion>(_region);
}

void
AudioRegionView::region_changed (const PropertyChange& what_changed)
{
	ENSURE_GUI_THREAD (*this, &AudioRegionView::region_changed, what_changed);

	RegionView::region_changed (what_changed);

	if (what_changed.contains (ARDOUR::Properties::scale_amplitude)) {
		region_scale_amplitude_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_in)) {
		fade_in_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_out)) {
		fade_out_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_in_active)) {
		fade_in_active_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_out_active)) {
		fade_out_active_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::envelope_active)) {
		envelope_active_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::valid_transients)) {
		transients_changed ();
	}
}

void
AudioRegionView::fade_in_changed ()
{
	reset_fade_in_shape ();
}

void
AudioRegionView::fade_out_changed ()
{
	reset_fade_out_shape ();
}

void
AudioRegionView::fade_in_active_changed ()
{
	if (start_xfade_rect) {
		if (audio_region()->fade_in_active()) {
			start_xfade_rect->set_fill (false);
		} else {
			start_xfade_rect->set_fill_color (UIConfiguration::instance().color_mod ("inactive crossfade", "inactive crossfade"));
			start_xfade_rect->set_fill (true);
		}
	}
}

void
AudioRegionView::fade_out_active_changed ()
{
	if (end_xfade_rect) {
		if (audio_region()->fade_out_active()) {
			end_xfade_rect->set_fill (false);
		} else {
			end_xfade_rect->set_fill_color (UIConfiguration::instance().color_mod ("inactive crossfade", "inactive crossfade"));
			end_xfade_rect->set_fill (true);
		}
	}
}


void
AudioRegionView::region_scale_amplitude_changed ()
{
	for (uint32_t n = 0; n < waves.size(); ++n) {
		waves[n]->gain_changed ();
	}
	region_renamed ();
}

void
AudioRegionView::region_renamed ()
{
	std::string str = RegionView::make_name ();

	if (audio_region()->speed_mismatch (trackview.session()->sample_rate())) {
		str = string ("*") + str;
	}

	if (_region->muted()) {
		str = string ("!") + str;
	}


	boost::shared_ptr<AudioRegion> ar (audio_region());
	if (ar->scale_amplitude() != 1.0) {
		char tmp[32];
		snprintf (tmp, 32, " (%.1fdB)", accurate_coefficient_to_dB (ar->scale_amplitude()));
		str += tmp;
	}

	set_item_name (str, this);
	set_name_text (str);
}

void
AudioRegionView::region_resized (const PropertyChange& what_changed)
{
	AudioGhostRegion* agr;

	RegionView::region_resized(what_changed);
	PropertyChange interesting_stuff;

	interesting_stuff.add (ARDOUR::Properties::start);
	interesting_stuff.add (ARDOUR::Properties::length);

	if (what_changed.contains (interesting_stuff)) {

		for (uint32_t n = 0; n < waves.size(); ++n) {
			waves[n]->region_resized ();
		}

		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
			if ((agr = dynamic_cast<AudioGhostRegion*>(*i)) != 0) {

				for (vector<ArdourWaveView::WaveView*>::iterator w = agr->waves.begin(); w != agr->waves.end(); ++w) {
					(*w)->region_resized ();
				}
			}
		}

		/* hide transient lines that extend beyond the region */
		list<std::pair<samplepos_t, ArdourCanvas::Line*> >::iterator l;
		samplepos_t first = _region->first_sample();
		samplepos_t last = _region->last_sample();

		for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {
			if (l->first < first || l->first >= last) {
				l->second->hide();
			} else {
				l->second->show();
			}
		}
	}
}

void
AudioRegionView::reset_width_dependent_items (double pixel_width)
{
	if (pixel_width == _width) {
		return;
	}

	RegionView::reset_width_dependent_items(pixel_width);
	assert(_pixel_width == pixel_width);

	pending_peak_data->set_x1(pixel_width);

	if (pixel_width <= 20.0 || _height < 5.0 || !trackview.session()->config.get_show_region_fades()) {
		if (fade_in_handle)       { fade_in_handle->hide(); }
		if (fade_out_handle)      { fade_out_handle->hide(); }
		if (fade_in_trim_handle)  { fade_in_trim_handle->hide(); }
		if (fade_out_trim_handle) { fade_out_trim_handle->hide(); }
		if (start_xfade_rect)     { start_xfade_rect->set_outline (false); }
		if (end_xfade_rect)       { end_xfade_rect->set_outline (false); }
	}

	reset_fade_shapes ();

	/* Update feature lines */
	AnalysisFeatureList analysis_features;
	_region->transients (analysis_features);

	if (feature_lines.size () != analysis_features.size ()) {
		cerr << "postponed freature line update.\n"; // XXX
		// AudioRegionView::transients_changed () will pick up on this
		return;
	}

	samplepos_t position = _region->position_sample();

	AnalysisFeatureList::const_iterator i;
	list<std::pair<samplepos_t, ArdourCanvas::Line*> >::iterator l;
	double y1;
	if (_height >= NAME_HIGHLIGHT_THRESH) {
		y1 = _height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 1;
	} else {
		y1 = _height - 1;
	}
	for (i = analysis_features.begin(), l = feature_lines.begin(); i != analysis_features.end() && l != feature_lines.end(); ++i, ++l) {
		float x_pos = trackview.editor().sample_to_pixel ((*i) - position);
		(*l).first = *i;
		(*l).second->set (ArdourCanvas::Duple (x_pos, 2.0),
				  ArdourCanvas::Duple (x_pos, y1));
	}
}

void
AudioRegionView::region_muted ()
{
	RegionView::region_muted();
	set_waveform_colors ();
}

void
AudioRegionView::setup_fade_handle_positions()
{
	/* position of fade handle offset from the top of the region view */
	double const handle_pos = 0.0;

	if (fade_in_handle) {
		fade_in_handle->set_y0 (handle_pos);
		fade_in_handle->set_y1 (handle_pos + handle_size);
	}

	if (fade_out_handle) {
		fade_out_handle->set_y0 (handle_pos);
		fade_out_handle->set_y1 (handle_pos + handle_size);
	}

	if (fade_in_trim_handle) {
		fade_in_trim_handle->set_y0 (_height - handle_size);
		fade_in_trim_handle->set_y1 (_height);
	}

	if (fade_out_trim_handle) {
		fade_out_trim_handle->set_y0 (_height - handle_size );
		fade_out_trim_handle->set_y1 (_height);
	}
}

void
AudioRegionView::set_height (gdouble height)
{
	uint32_t gap = UIConfiguration::instance().get_vertical_region_gap ();
	float ui_scale = UIConfiguration::instance().get_ui_scale ();
	if (gap > 0 && ui_scale > 0) {
		gap = ceil (gap * ui_scale);
	}

	height = std::max (3.0, height - gap);

	if (height == _height) {
		return;
	}

	RegionView::set_height (height);
	pending_peak_data->set_y1 (height);

	RouteTimeAxisView& atv (*(dynamic_cast<RouteTimeAxisView*>(&trackview))); // ick
	uint32_t nchans = atv.track()->n_channels().n_audio();

	if (!tmp_waves.empty () || !waves.empty ()) {

		gdouble ht;

		if (!UIConfiguration::instance().get_show_name_highlight() || (height < NAME_HIGHLIGHT_THRESH)) {
			ht = height / (double) nchans;
		} else {
			ht = (height - NAME_HIGHLIGHT_SIZE) / (double) nchans;
		}

		uint32_t wcnt = waves.size();
		for (uint32_t n = 0; n < wcnt; ++n) {
			gdouble yoff = floor (ht * n);
			waves[n]->set_height (ht);
			waves[n]->set_y_position (yoff);
		}

		wcnt = tmp_waves.size();
		for (uint32_t n = 0; n < wcnt; ++n) {
			if (!tmp_waves[n]) {
				continue;
			}
			gdouble yoff = floor (ht * n);
			tmp_waves[n]->set_height (ht);
			tmp_waves[n]->set_y_position (yoff);
		}
	}

	if (gain_line) {

		if ((height / nchans) < NAME_HIGHLIGHT_THRESH) {
			gain_line->hide ();
		} else {
			update_envelope_visibility ();
		}

		gain_line->set_height ((uint32_t) rint (height - NAME_HIGHLIGHT_SIZE) - 2);
	}

	reset_fade_shapes ();

	/* Update heights for any feature lines */
	samplepos_t position = _region->position_sample();
	list<std::pair<samplepos_t, ArdourCanvas::Line*> >::iterator l;
	double y1;
	if (_height >= NAME_HIGHLIGHT_THRESH) {
		y1 = _height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 1;
	} else {
		y1 = _height - 1;
	}
	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {
		float pos_x = trackview.editor().sample_to_pixel((*l).first - position);
		(*l).second->set (ArdourCanvas::Duple (pos_x, 2.0),
				ArdourCanvas::Duple (pos_x, y1));
	}

	if (name_text) {
		name_text->raise_to_top();
	}

	setup_fade_handle_positions();
}

void
AudioRegionView::reset_fade_shapes ()
{
	if (!trim_fade_in_drag_active) { reset_fade_in_shape (); }
	if (!trim_fade_out_drag_active) { reset_fade_out_shape (); }
}

void
AudioRegionView::reset_fade_in_shape ()
{
	reset_fade_in_shape_width (audio_region(), audio_region()->fade_in()->back()->when.samples());
}

void
AudioRegionView::reset_fade_in_shape_width (boost::shared_ptr<AudioRegion> ar, samplecnt_t width, bool drag_active)
{
	trim_fade_in_drag_active = drag_active;
	if (fade_in_handle == 0) {
		return;
	}

	/* smallest size for a fade is 64 samples */

	width = std::max ((samplecnt_t) 64, width);

	/* round here to prevent little visual glitches with sub-pixel placement */
	double const pwidth  = (double) width /  samples_per_pixel;
	double const handle_left = pwidth;

	/* Put the fade in handle so that its left side is at the end-of-fade line */
	fade_in_handle->set_x0 (handle_left);
	fade_in_handle->set_x1 (handle_left + handle_size);

	if (fade_in_trim_handle) {
		fade_in_trim_handle->set_x0 (0);
		fade_in_trim_handle->set_x1 (handle_size);
	}

	if (fade_in_handle->visible()) {
		//see comment for drag_start
		entered();
	}

	if (pwidth < 5.f) {
		hide_start_xfade();
		return;
	}

	if (!trackview.session()->config.get_show_region_fades()) {
		hide_start_xfade ();
		return;
	}

	double effective_height;

	if (_height >= NAME_HIGHLIGHT_THRESH) {
		effective_height = _height - NAME_HIGHLIGHT_SIZE;
	} else {
		effective_height = _height;
	}

	/* points *MUST* be in anti-clockwise order */

	Points points;
	Points::size_type pi;
	boost::shared_ptr<const Evoral::ControlList> list (audio_region()->fade_in());
	Evoral::ControlList::const_iterator x;
	samplecnt_t length = list->length().samples();

	points.assign (list->size(), Duple());

	for (x = list->begin(), pi = 0; x != list->end(); ++x, ++pi) {
		const double p = (*x)->when.samples();
		points[pi].x = (p * pwidth) / length;
		points[pi].y = effective_height - ((*x)->value * (effective_height - 1.));
	}

	/* draw the line */

	redraw_start_xfade_to (ar, width, points, effective_height, handle_left);

	/* ensure trim handle stays on top */
	if (frame_handle_start) {
		frame_handle_start->raise_to_top();
	}
}

void
AudioRegionView::reset_fade_out_shape ()
{
	reset_fade_out_shape_width (audio_region(), audio_region()->fade_out()->back()->when.samples());
}

void
AudioRegionView::reset_fade_out_shape_width (boost::shared_ptr<AudioRegion> ar, samplecnt_t width, bool drag_active)
{
	trim_fade_out_drag_active = drag_active;
	if (fade_out_handle == 0) {
		return;
	}

	/* smallest size for a fade is 64 samples */

	width = std::max ((samplecnt_t) 64, width);


	double const pwidth = floor(trackview.editor().sample_to_pixel (width));

	/* the right edge should be right on the region frame is the pixel
	 * width is zero. Hence the additional + 1.0 at the end.
	 */

	double const handle_right = rint(trackview.editor().sample_to_pixel (_region->length_samples()) - pwidth);
	double const trim_handle_right = rint(trackview.editor().sample_to_pixel (_region->length_samples()));

	/* Put the fade out handle so that its right side is at the end-of-fade line;
	 */
	fade_out_handle->set_x0 (handle_right - handle_size);
	fade_out_handle->set_x1 (handle_right);
	if (fade_out_trim_handle) {
		fade_out_trim_handle->set_x0 (1 + trim_handle_right - handle_size);
		fade_out_trim_handle->set_x1 (1 + trim_handle_right);
	}

	if (fade_out_handle->visible()) {
		//see comment for drag_start
		entered();
	}
	/* don't show shape if its too small */

	if (pwidth < 5) {
		hide_end_xfade();
		return;
	}

	if (!trackview.session()->config.get_show_region_fades()) {
		hide_end_xfade();
		return;
	}

	double effective_height;

	effective_height = _height;

	if (UIConfiguration::instance().get_show_name_highlight() && effective_height >= NAME_HIGHLIGHT_THRESH) {
		effective_height -= NAME_HIGHLIGHT_SIZE;
	}

	/* points *MUST* be in anti-clockwise order */

	Points points;
	Points::size_type pi;
	boost::shared_ptr<const Evoral::ControlList> list (audio_region()->fade_out());
	Evoral::ControlList::const_iterator x;
	double length = list->length().samples();

	points.assign (list->size(), Duple());

	for (x = list->begin(), pi = 0; x != list->end(); ++x, ++pi) {
		const double p = (*x)->when.samples();
		points[pi].x = _pixel_width - pwidth + (pwidth * (p/length));
		points[pi].y = effective_height - ((*x)->value * (effective_height - 1.));
	}

	/* draw the line */

	redraw_end_xfade_to (ar, width, points, effective_height, handle_right, pwidth);

	/* ensure trim handle stays on top */
	if (frame_handle_end) {
		frame_handle_end->raise_to_top();
	}
}

samplepos_t
AudioRegionView::get_fade_in_shape_width ()
{
	return audio_region()->fade_in()->back()->when.samples();
}

samplepos_t
AudioRegionView::get_fade_out_shape_width ()
{
	return audio_region()->fade_out()->back()->when.samples();
}


void
AudioRegionView::redraw_start_xfade ()
{
	boost::shared_ptr<AudioRegion> ar (audio_region());

	if (!ar->fade_in() || ar->fade_in()->empty()) {
		return;
	}

	show_start_xfade();
	reset_fade_in_shape_width (ar, ar->fade_in()->back()->when.samples());
}

void
AudioRegionView::redraw_start_xfade_to (boost::shared_ptr<AudioRegion> ar, samplecnt_t /*width*/, Points& points, double effective_height,
					double rect_width)
{
	if (points.size() < 2) {
		return;
	}

	if (!start_xfade_curve) {
		start_xfade_curve = new ArdourCanvas::XFadeCurve (group, ArdourCanvas::XFadeCurve::Start);
		CANVAS_DEBUG_NAME (start_xfade_curve, string_compose ("xfade start out line for %1", region()->name()));
		start_xfade_curve->set_fill_color (UIConfiguration::instance().color_mod ("active crossfade", "crossfade alpha"));
		start_xfade_curve->set_outline_color (UIConfiguration::instance().color ("crossfade line"));
		start_xfade_curve->set_ignore_events (true);
	}
	if (!start_xfade_rect) {
		start_xfade_rect = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (start_xfade_rect, string_compose ("xfade start rect for %1", region()->name()));
		start_xfade_rect->set_outline_color (UIConfiguration::instance().color ("crossfade line"));
		start_xfade_rect->set_fill (false);
		start_xfade_rect->set_outline (false);
		start_xfade_rect->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_start_xfade_event), start_xfade_rect, this));
		start_xfade_rect->set_data ("regionview", this);
	}

	start_xfade_rect->set (ArdourCanvas::Rect (0.0, 0.0, rect_width, effective_height));

	/* fade out line */

	boost::shared_ptr<AutomationList> inverse = ar->inverse_fade_in ();
	Points ipoints;
	Points::size_type npoints;

	if (!inverse) {

		/* there is no explicit inverse fade in curve, so take the
		 * regular fade in curve given to use as "points" (already a
		 * set of coordinates), and convert to the inverse shape.
		 */

		npoints = points.size();
		ipoints.assign (npoints, Duple());

		for (Points::size_type i = 0, pci = 0; i < npoints; ++i, ++pci) {
			ArdourCanvas::Duple &p (ipoints[pci]);
			/* leave x-axis alone but invert with respect to y-axis */
			p.y = effective_height - points[pci].y;
		}

	} else {

		/* there is an explicit inverse fade in curve. Grab the points
		   and convert them into coordinates for the inverse fade in
		   line.
		*/

		npoints = inverse->size();
		ipoints.assign (npoints, Duple());

		Evoral::ControlList::const_iterator x;
		Points::size_type pi;
		const double length = inverse->length().samples();

		for (x = inverse->begin(), pi = 0; x != inverse->end(); ++x, ++pi) {
			ArdourCanvas::Duple& p (ipoints[pi]);
			double pos = (*x)->when.samples();
			p.x = (rect_width * (pos/length));
			p.y = effective_height - ((*x)->value * (effective_height));
		}
	}

	start_xfade_curve->set_inout (points, ipoints);

	show_start_xfade();
}

void
AudioRegionView::redraw_end_xfade ()
{
	boost::shared_ptr<AudioRegion> ar (audio_region());

	if (!ar->fade_out() || ar->fade_out()->empty()) {
		return;
	}

	show_end_xfade();

	reset_fade_out_shape_width (ar, ar->fade_out()->back()->when.samples());
}

void
AudioRegionView::redraw_end_xfade_to (boost::shared_ptr<AudioRegion> ar, samplecnt_t width, Points& points, double effective_height,
                                      double rect_edge, double rect_width)
{
	if (points.size() < 2) {
		return;
	}

	if (!end_xfade_curve) {
		end_xfade_curve = new ArdourCanvas::XFadeCurve (group, ArdourCanvas::XFadeCurve::End);
		CANVAS_DEBUG_NAME (end_xfade_curve, string_compose ("xfade end out line for %1", region()->name()));
		end_xfade_curve->set_fill_color (UIConfiguration::instance().color_mod ("active crossfade", "crossfade alpha"));
		end_xfade_curve->set_outline_color (UIConfiguration::instance().color ("crossfade line"));
		end_xfade_curve->set_ignore_events (true);
	}

	if (!end_xfade_rect) {
		end_xfade_rect = new ArdourCanvas::Rectangle (group);
		CANVAS_DEBUG_NAME (end_xfade_rect, string_compose ("xfade end rect for %1", region()->name()));
		end_xfade_rect->set_outline_color (UIConfiguration::instance().color ("crossfade line"));
		end_xfade_rect->set_fill (false);
		end_xfade_rect->set_outline (false);
		end_xfade_rect->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_end_xfade_event), end_xfade_rect, this));
		end_xfade_rect->set_data ("regionview", this);
	}

	end_xfade_rect->set (ArdourCanvas::Rect (rect_edge, 0.0, rect_edge + rect_width, effective_height));

	/* fade in line */

	boost::shared_ptr<AutomationList> inverse = ar->inverse_fade_out ();
	Points ipoints;
	Points::size_type npoints;

	if (!inverse) {

		/* there is no explicit inverse fade out curve, so take the
		 * regular fade out curve given to use as "points" (already a
		 * set of coordinates), and convert to the inverse shape.
		 */

		npoints = points.size();
		ipoints.assign (npoints, Duple());

		Points::size_type pci;

		for (pci = 0; pci < npoints; ++pci) {
			ArdourCanvas::Duple &p (ipoints[pci]);
			p.y = effective_height - points[pci].y;
		}

	} else {

		/* there is an explicit inverse fade out curve. Grab the points
		   and convert them into coordinates for the inverse fade out
		   line.
		*/

		npoints = inverse->size();
		ipoints.assign (npoints, Duple());

		const double rend = trackview.editor().sample_to_pixel (_region->length_samples() - width);

		Evoral::ControlList::const_iterator x;
		Points::size_type pi;
		const double length = inverse->length().samples();

		for (x = inverse->begin(), pi = 0; x != inverse->end(); ++x, ++pi) {
			ArdourCanvas::Duple& p (ipoints[pi]);
			const double pos = (*x)->when.samples();
			p.x = (rect_width * (pos/length)) + rend;
			p.y = effective_height - ((*x)->value * (effective_height));
		}
	}

	end_xfade_curve->set_inout (ipoints, points);

	show_end_xfade();
}

void
AudioRegionView::hide_xfades ()
{
	hide_start_xfade ();
	hide_end_xfade ();
}

void
AudioRegionView::hide_start_xfade ()
{
	if (start_xfade_curve) {
		start_xfade_curve->hide();
	}
	if (start_xfade_rect) {
		start_xfade_rect->hide ();
	}

	_start_xfade_visible = false;
}

void
AudioRegionView::hide_end_xfade ()
{
	if (end_xfade_curve) {
		end_xfade_curve->hide();
	}
	if (end_xfade_rect) {
		end_xfade_rect->hide ();
	}

	_end_xfade_visible = false;
}

void
AudioRegionView::show_start_xfade ()
{
	if (start_xfade_curve) {
		start_xfade_curve->show();
	}
	if (start_xfade_rect) {
		start_xfade_rect->show ();
	}

	_start_xfade_visible = true;
}

void
AudioRegionView::show_end_xfade ()
{
	if (end_xfade_curve) {
		end_xfade_curve->show();
	}
	if (end_xfade_rect) {
		end_xfade_rect->show ();
	}

	_end_xfade_visible = true;
}

void
AudioRegionView::set_samples_per_pixel (gdouble fpp)
{
	RegionView::set_samples_per_pixel (fpp);

	if (UIConfiguration::instance().get_show_waveforms ()) {
		for (uint32_t n = 0; n < waves.size(); ++n) {
			waves[n]->set_samples_per_pixel (fpp);
		}
	}

	if (gain_line) {
		gain_line->reset ();
	}

	reset_fade_shapes ();
}

void
AudioRegionView::set_amplitude_above_axis (gdouble a)
{
	for (uint32_t n=0; n < waves.size(); ++n) {
		waves[n]->set_amplitude_above_axis (a);
	}
}

void
AudioRegionView::set_colors ()
{
	RegionView::set_colors();

	if (gain_line) {
		gain_line->set_line_color (audio_region()->envelope_active() ?
					   UIConfiguration::instance().color ("gain line") :
					   UIConfiguration::instance().color_mod ("gain line inactive", "gain line inactive"));
	}

	set_waveform_colors ();

	if (start_xfade_curve) {
		start_xfade_curve->set_fill_color (UIConfiguration::instance().color_mod ("active crossfade", "crossfade alpha"));
		start_xfade_curve->set_outline_color (UIConfiguration::instance().color ("crossfade line"));
	}
	if (end_xfade_curve) {
		end_xfade_curve->set_fill_color (UIConfiguration::instance().color_mod ("active crossfade", "crossfade alpha"));
		end_xfade_curve->set_outline_color (UIConfiguration::instance().color ("crossfade line"));
	}

	if (start_xfade_rect) {
		start_xfade_rect->set_outline_color (UIConfiguration::instance().color ("crossfade line"));
	}
	if (end_xfade_rect) {
		end_xfade_rect->set_outline_color (UIConfiguration::instance().color ("crossfade line"));
	}
}

void
AudioRegionView::setup_waveform_visibility ()
{
	if (UIConfiguration::instance().get_show_waveforms ()) {
		for (uint32_t n = 0; n < waves.size(); ++n) {
			/* make sure the zoom level is correct, since we don't update
			   this when waveforms are hidden.
			*/
			// CAIROCANVAS
			// waves[n]->set_samples_per_pixel (_samples_per_pixel);
			waves[n]->show();
		}
	} else {
		for (uint32_t n = 0; n < waves.size(); ++n) {
			waves[n]->hide();
		}
	}
}

void
AudioRegionView::temporarily_hide_envelope ()
{
	if (gain_line) {
		gain_line->hide ();
	}
}

void
AudioRegionView::unhide_envelope ()
{
	update_envelope_visibility ();
}

void
AudioRegionView::update_envelope_visibility ()
{
	if (!gain_line) {
		return;
	}

	if (trackview.editor().current_mouse_mode() == Editing::MouseDraw || trackview.editor().current_mouse_mode() == Editing::MouseContent ) {
		gain_line->set_visibility (AutomationLine::VisibleAspects(AutomationLine::ControlPoints|AutomationLine::Line));
		gain_line->canvas_group().raise_to_top ();
	} else if (UIConfiguration::instance().get_show_region_gain() || trackview.editor().current_mouse_mode() == Editing::MouseRange ) {
		gain_line->set_visibility (AutomationLine::VisibleAspects(AutomationLine::Line));
		gain_line->canvas_group().raise_to_top ();
	} else {
		gain_line->set_visibility (AutomationLine::VisibleAspects(0));
	}
}

void
AudioRegionView::delete_waves ()
{
	for (vector<ScopedConnection*>::iterator i = _data_ready_connections.begin(); i != _data_ready_connections.end(); ++i) {
		delete *i;
	}
	_data_ready_connections.clear ();

	for (vector<ArdourWaveView::WaveView*>::iterator w = waves.begin(); w != waves.end(); ++w) {
		group->remove(*w);
		delete *w;
	}
	waves.clear();

	while (!tmp_waves.empty ()) {
		delete tmp_waves.back ();
		tmp_waves.pop_back ();
	}
	pending_peak_data->show ();
}

void
AudioRegionView::create_waves ()
{
	// cerr << "AudioRegionView::create_waves() called on " << this << endl;//DEBUG
	RouteTimeAxisView& atv (*(dynamic_cast<RouteTimeAxisView*>(&trackview))); // ick

	if (!atv.track()) {
		return;
	}

	ChanCount nchans = atv.track()->n_channels();

	// cerr << "creating waves for " << _region->name() << " with wfd = " << wait_for_data
	//		<< " and channels = " << nchans.n_audio() << endl;

	/* in tmp_waves, set up null pointers for each channel so the vector is allocated */
	for (uint32_t n = 0; n < nchans.n_audio(); ++n) {
		tmp_waves.push_back (0);
	}

	for (vector<ScopedConnection*>::iterator i = _data_ready_connections.begin(); i != _data_ready_connections.end(); ++i) {
		delete *i;
	}

	_data_ready_connections.clear ();

	for (uint32_t i = 0; i < nchans.n_audio(); ++i) {
		_data_ready_connections.push_back (0);
	}

	for (uint32_t n = 0; n < nchans.n_audio(); ++n) {

		if (n >= audio_region()->n_channels()) {
			break;
		}

		// cerr << "\tchannel " << n << endl;

		if (wait_for_data) {
			if (audio_region()->audio_source(n)->peaks_ready (boost::bind (&AudioRegionView::peaks_ready_handler, this, n), &_data_ready_connections[n], gui_context())) {
				// cerr << "\tData is ready for channel " << n << "\n";
				create_one_wave (n, true);
			} else {
				// cerr << "\tdata is not ready for channel " << n << "\n";
				// we'll get a PeaksReady signal from the source in the future
				// and will call create_one_wave(n) then.
				pending_peak_data->show ();
			}

		} else {
			// cerr << "\tdon't delay, display channel " << n << " today!\n";
			create_one_wave (n, true);
		}

	}
}

void
AudioRegionView::create_one_wave (uint32_t which, bool /*direct*/)
{
	//cerr << "AudioRegionView::create_one_wave() called which: " << which << " this: " << this << endl;//DEBUG
	RouteTimeAxisView& atv (*(dynamic_cast<RouteTimeAxisView*>(&trackview))); // ick
	if (!trackview.session() || trackview.session()->deletion_in_progress () || !atv.track()) {
		/* peaks_ready_handler() may be called from peak_thread_work() while
		 * session deletion is in progress.
		 * Since session-unload happens in the GUI thread, we need to test
		 * in this context.
		 */
		return;
	}
	uint32_t nchans = atv.track()->n_channels().n_audio();
	uint32_t n;
	uint32_t nwaves = std::min (nchans, audio_region()->n_channels());
	gdouble ht;

	/* compare to set_height(), use _height as set by streamview (child_height),
	 * not trackview.current_height() to take stacked layering into acconnt
	 */
	if (!UIConfiguration::instance().get_show_name_highlight() || (_height < NAME_HIGHLIGHT_THRESH)) {
		ht = _height / (double) nchans;
	} else {
		ht = (_height - NAME_HIGHLIGHT_SIZE) / (double) nchans;
	}

	/* first waveview starts at 1.0, not 0.0 since that will overlap the frame */
	gdouble yoff = which * ht;

	ArdourWaveView::WaveView *wave = new ArdourWaveView::WaveView (group, audio_region ());
	CANVAS_DEBUG_NAME (wave, string_compose ("wave view for chn %1 of %2", which, get_item_name()));

	wave->set_channel (which);
	wave->set_y_position (yoff);
	wave->set_height (ht);
	wave->set_samples_per_pixel (samples_per_pixel);
	wave->set_show_zero_line (true);
	wave->set_clip_level (UIConfiguration::instance().get_waveform_clip_level ());
	wave->set_start_shift (1.0);

	wave->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_wave_view_event), wave, this));

	switch (UIConfiguration::instance().get_waveform_shape()) {
	case Rectified:
		wave->set_shape (ArdourWaveView::WaveView::Rectified);
		break;
	default:
		wave->set_shape (ArdourWaveView::WaveView::Normal);
	}

	wave->set_logscaled (UIConfiguration::instance().get_waveform_scale() == Logarithmic);

	vector<ArdourWaveView::WaveView*> v;
	v.push_back (wave);
	set_some_waveform_colors (v);

	if (!UIConfiguration::instance().get_show_waveforms ()) {
		wave->hide();
	}

	/* note: calling this function is serialized by the lock
	   held in the peak building thread that signals that
	   peaks are ready for use *or* by the fact that it is
	   called one by one from the GUI thread.
	*/

	if (which < nchans) {
		tmp_waves[which] = wave;
	} else {
		/* n-channel track, >n-channel source */
	}

	/* see if we're all ready */

	for (n = 0; n < nchans; ++n) {
		if (tmp_waves[n] == 0) {
			break;
		}
	}

	if (n == nwaves) {
		/* all waves are ready */
		tmp_waves.resize(nwaves);

		waves.swap(tmp_waves);

		while (!tmp_waves.empty ()) {
			delete tmp_waves.back ();
			tmp_waves.pop_back ();
		}

		/* indicate peak-completed */
		pending_peak_data->hide ();

		/* Restore stacked coverage */
		LayerDisplay layer_display;
		if (trackview.get_gui_property ("layer-display", layer_display)) {
			update_coverage_frame (layer_display);
	  }
	}

	/* channel wave created, don't hook into peaks ready anymore */
	delete _data_ready_connections[which];
	_data_ready_connections[which] = 0;

	maybe_raise_cue_markers ();
}

void
AudioRegionView::peaks_ready_handler (uint32_t which)
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&AudioRegionView::create_one_wave, this, which, false));
	// cerr << "AudioRegionView::peaks_ready_handler() called on " << which << " this: " << this << endl;
}

void
AudioRegionView::add_gain_point_event (ArdourCanvas::Item *item, GdkEvent *ev, bool with_guard_points)
{
	if (!gain_line) {
		return;
	}

	uint32_t before_p, after_p;
	double mx = ev->button.x;
	double my = ev->button.y;

	item->canvas_to_item (mx, my);

	samplecnt_t const sample_within_region = (samplecnt_t) floor (mx * samples_per_pixel);

	if (!gain_line->control_points_adjacent (sample_within_region, before_p, after_p)) {
		/* no adjacent points */
		return;
	}

	/* y is in item frame */
	double const bx = gain_line->nth (before_p)->get_x();
	double const ax = gain_line->nth (after_p)->get_x();
	double const click_ratio = (ax - mx) / (ax - bx);

	double y = ((gain_line->nth (before_p)->get_y() * click_ratio) + (gain_line->nth (after_p)->get_y() * (1 - click_ratio)));

	/* don't create points that can't be seen */

	update_envelope_visibility ();
	samplepos_t rpos = region ()->position_sample();
	timepos_t snap_pos = timepos_t (trackview.editor().pixel_to_sample (mx) + rpos);
	trackview.editor ().snap_to_with_modifier (snap_pos, ev);
	samplepos_t fx = snap_pos.samples() - rpos;

	if (fx > _region->length_samples()) {
		return;
	}

	/* compute vertical fractional position */

	y = 1.0 - (y / (gain_line->height()));

	/* map using gain line */

	gain_line->view_to_model_coord (mx, y);

	/* XXX STATEFUL: can't convert to stateful diff until we
	   can represent automation data with it.
	*/

	XMLNode &before = audio_region()->envelope()->get_state();
	MementoCommand<AudioRegion>* region_memento = 0;

	if (!audio_region()->envelope_active()) {
		XMLNode &region_before = audio_region()->get_state();
		audio_region()->set_envelope_active(true);
		XMLNode &region_after = audio_region()->get_state();
		region_memento = new MementoCommand<AudioRegion>(*(audio_region().get()), &region_before, &region_after);
	}

	if (audio_region()->envelope()->editor_add (timepos_t (fx), y, with_guard_points)) {
		XMLNode &after = audio_region()->envelope()->get_state();
		std::list<Selectable*> results;

		trackview.editor().begin_reversible_command (_("add gain control point"));

		if (region_memento) {
			trackview.session()->add_command (region_memento);
		}

		trackview.session()->add_command (new MementoCommand<AutomationList>(*audio_region()->envelope().get(), &before, &after));

		gain_line->get_selectables (region ()->nt_position () + timecnt_t (fx), region ()->nt_position () + timecnt_t (fx), 0.0, 1.0, results);
		trackview.editor ().get_selection ().set (results);

		trackview.editor ().commit_reversible_command ();
		trackview.session ()->set_dirty ();
	} else {
		delete region_memento;
	}
}

void
AudioRegionView::remove_gain_point_event (ArdourCanvas::Item *item, GdkEvent* /*ev*/)
{
	ControlPoint *cp = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"));
	audio_region()->envelope()->erase (cp->model());
}

GhostRegion*
AudioRegionView::add_ghost (TimeAxisView& tv)
{
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&trackview);

	if (!rtv) {
		return 0;
	}

	double unit_position = _region->position_sample () / samples_per_pixel;
	AudioGhostRegion* ghost = new AudioGhostRegion (*this, tv, trackview, unit_position);
	uint32_t nchans;

	nchans = rtv->track()->n_channels().n_audio();

	for (uint32_t n = 0; n < nchans; ++n) {

		if (n >= audio_region()->n_channels()) {
			break;
		}

		ArdourWaveView::WaveView *wave = new ArdourWaveView::WaveView (ghost->group, audio_region());
		CANVAS_DEBUG_NAME (wave, string_compose ("ghost wave for %1", get_item_name()));

		wave->set_channel (n);
		wave->set_samples_per_pixel (samples_per_pixel);
		wave->set_amplitude_above_axis (_amplitude_above_axis);

		ghost->waves.push_back(wave);
	}

	ghost->set_height ();
	ghost->set_duration (_region->length_samples() / samples_per_pixel);
	ghost->set_colors();
	ghosts.push_back (ghost);

	return ghost;
}

void
AudioRegionView::entered ()
{
	trackview.editor().set_current_trimmable (_region);
	trackview.editor().set_current_movable (_region);

	update_envelope_visibility();

	if ((trackview.editor().current_mouse_mode() == Editing::MouseObject)) {
		if (start_xfade_rect) {
			start_xfade_rect->set_outline (true);
		}
		if (end_xfade_rect) {
			end_xfade_rect->set_outline (true);
		}
		if (fade_in_handle) {
			fade_in_handle->show ();
			fade_in_handle->raise_to_top ();
		}
		if (fade_out_handle) {
			fade_out_handle->show ();
			fade_out_handle->raise_to_top ();
		}
		if (fade_in_trim_handle) {
			boost::shared_ptr<AudioRegion> ar (audio_region());
			if (!ar->locked() && (ar->fade_in()->back()->when > 64 || (ar->can_trim() & Trimmable::FrontTrimEarlier))) {
				fade_in_trim_handle->show ();
				fade_in_trim_handle->raise_to_top ();
			} else {
				fade_in_trim_handle->hide ();
			}
		}
		if (fade_out_trim_handle) {
			boost::shared_ptr<AudioRegion> ar (audio_region());
			if (!ar->locked() && (ar->fade_out()->back()->when > 64 || (ar->can_trim() & Trimmable::EndTrimLater))) {
				fade_out_trim_handle->show ();
				fade_out_trim_handle->raise_to_top ();
			} else {
				fade_out_trim_handle->hide ();
			}
		}
	} else {  //this happens when we switch tools; if we switch away from Grab mode,  hide all the fade handles
		if (fade_in_handle)       { fade_in_handle->hide(); }
		if (fade_out_handle)      { fade_out_handle->hide(); }
		if (fade_in_trim_handle)  { fade_in_trim_handle->hide(); }
		if (fade_out_trim_handle) { fade_out_trim_handle->hide(); }
		if (start_xfade_rect)     { start_xfade_rect->set_outline (false); }
		if (end_xfade_rect)       { end_xfade_rect->set_outline (false); }
	}
}

void
AudioRegionView::exited ()
{
	trackview.editor().set_current_trimmable (boost::shared_ptr<Trimmable>());
	trackview.editor().set_current_movable (boost::shared_ptr<Movable>());

//	if (gain_line) {
//		gain_line->remove_visibility (AutomationLine::ControlPoints);
//	}

	if (fade_in_handle)       { fade_in_handle->hide(); }
	if (fade_out_handle)      { fade_out_handle->hide(); }
	if (fade_in_trim_handle)  { fade_in_trim_handle->hide(); }
	if (fade_out_trim_handle) { fade_out_trim_handle->hide(); }
	if (start_xfade_rect)     { start_xfade_rect->set_outline (false); }
	if (end_xfade_rect)       { end_xfade_rect->set_outline (false); }
}

void
AudioRegionView::envelope_active_changed ()
{
	if (gain_line) {
		gain_line->set_line_color (audio_region()->envelope_active() ?
					   UIConfiguration::instance().color ("gain line") :
					   UIConfiguration::instance().color_mod ("gain line inactive", "gain line inactive"));
		update_envelope_visibility ();
	}
}

void
AudioRegionView::color_handler ()
{
	//case cMutedWaveForm:
	//case cWaveForm:
	//case cWaveFormClip:
	//case cZeroLine:
	set_colors ();

	//case cGainLineInactive:
	//case cGainLine:
	envelope_active_changed();

}

void
AudioRegionView::set_waveform_colors ()
{
	set_some_waveform_colors (waves);
}

void
AudioRegionView::set_some_waveform_colors (vector<ArdourWaveView::WaveView*>& waves_to_color)
{
	Gtkmm2ext::Color fill = fill_color;
	Gtkmm2ext::Color outline = fill;

	Gtkmm2ext::Color clip = UIConfiguration::instance().color ("clipped waveform");
	Gtkmm2ext::Color zero = UIConfiguration::instance().color ("zero line");

	/* use track/region color to fill wform */
	fill = fill_color;
	fill = UINT_INTERPOLATE (fill, UIConfiguration::instance().color ("waveform fill"), 0.5);

	/* set outline */
	outline = UIConfiguration::instance().color ("waveform outline");

	if (_selected) {
		outline = UINT_RGBA_CHANGE_A(UIConfiguration::instance().color ("selected waveform outline"), 0xC0);
		fill = UINT_RGBA_CHANGE_A(UIConfiguration::instance().color ("selected waveform fill"), 0xC0);
	} else if (_dragging) {
		outline = UINT_RGBA_CHANGE_A(UIConfiguration::instance().color ("waveform outline"), 0xC0);
		fill = UINT_RGBA_CHANGE_A(UIConfiguration::instance().color ("waveform fill"), 0xC0);
	} else if (_region->muted()) {
		outline = UINT_RGBA_CHANGE_A(UIConfiguration::instance().color ("waveform outline"), 80);
		fill = UINT_RGBA_CHANGE_A(UIConfiguration::instance().color ("waveform fill"), 0);
	} else if (!_region->opaque()) {
		outline = UINT_RGBA_CHANGE_A(UIConfiguration::instance().color ("waveform outline"), 70);
		fill = UINT_RGBA_CHANGE_A(UIConfiguration::instance().color ("waveform fill"), 70);
	}

	/* recorded region, override to red */
	if (_recregion) {
		outline = UIConfiguration::instance().color ("recording waveform outline");
		fill = UIConfiguration::instance().color ("recording waveform fill");
	}

	for (vector<ArdourWaveView::WaveView*>::iterator w = waves_to_color.begin(); w != waves_to_color.end(); ++w) {
		(*w)->set_fill_color (fill);
		(*w)->set_outline_color (outline);
		(*w)->set_clip_color (clip);
		(*w)->set_zero_color (zero);
	}
}

void
AudioRegionView::set_frame_color ()
{
	if (!frame) {
		return;
	}

	RegionView::set_frame_color ();

	set_waveform_colors ();
}

void
AudioRegionView::set_fade_visibility (bool yn)
{
	if (yn) {
		if (start_xfade_curve)    { start_xfade_curve->show (); }
		if (end_xfade_curve)      { end_xfade_curve->show (); }
		if (start_xfade_rect)     { start_xfade_rect->show (); }
		if (end_xfade_rect)       { end_xfade_rect->show (); }
		} else {
		if (start_xfade_curve)    { start_xfade_curve->hide(); }
		if (end_xfade_curve)      { end_xfade_curve->hide(); }
		if (fade_in_handle)       { fade_in_handle->hide(); }
		if (fade_out_handle)      { fade_out_handle->hide(); }
		if (fade_in_trim_handle)  { fade_in_trim_handle->hide(); }
		if (fade_out_trim_handle) { fade_out_trim_handle->hide(); }
		if (start_xfade_rect)     { start_xfade_rect->hide (); }
		if (end_xfade_rect)       { end_xfade_rect->hide (); }
		if (start_xfade_rect)     { start_xfade_rect->set_outline (false); }
		if (end_xfade_rect)       { end_xfade_rect->set_outline (false); }
	}
}

void
AudioRegionView::update_coverage_frame (LayerDisplay d)
{
	RegionView::update_coverage_frame (d);

	if (d == Stacked) {
		if (fade_in_handle)       { fade_in_handle->raise_to_top (); }
		if (fade_out_handle)      { fade_out_handle->raise_to_top (); }
		if (fade_in_trim_handle)  { fade_in_trim_handle->raise_to_top (); }
		if (fade_out_trim_handle) { fade_out_trim_handle->raise_to_top (); }
	}
}

void
AudioRegionView::show_region_editor ()
{
	if (editor == 0) {
		editor = new AudioRegionEditor (trackview.session(), audio_region());
	}

	editor->present ();
	editor->show_all();
}

void
AudioRegionView::transients_changed ()
{
	AnalysisFeatureList analysis_features;
	_region->transients (analysis_features);
	samplepos_t position = _region->position_sample();
	samplepos_t first = _region->first_sample();
	samplepos_t last = _region->last_sample();

	double y1;
	if (_height >= NAME_HIGHLIGHT_THRESH) {
		y1 = _height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 1;
	} else {
		y1 = _height - 1;
	}

	while (feature_lines.size() < analysis_features.size()) {
		ArdourCanvas::Line* canvas_item = new ArdourCanvas::Line(group);
		CANVAS_DEBUG_NAME (canvas_item, string_compose ("transient group for %1", region()->name()));
		canvas_item->set_outline_color (UIConfiguration::instance().color ("zero line")); // also in Editor::leave_handler()

		canvas_item->set (ArdourCanvas::Duple (-1.0, 2.0),
				  ArdourCanvas::Duple (1.0, y1));

		canvas_item->raise_to_top ();
		canvas_item->show ();

		canvas_item->set_data ("regionview", this);
		canvas_item->Event.connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_feature_line_event), canvas_item, this));

		feature_lines.push_back (make_pair(0, canvas_item));
	}

	while (feature_lines.size() > analysis_features.size()) {
		ArdourCanvas::Line* line = feature_lines.back().second;
		feature_lines.pop_back ();
		delete line;
	}

	AnalysisFeatureList::const_iterator i;
	list<std::pair<samplepos_t, ArdourCanvas::Line*> >::iterator l;

	for (i = analysis_features.begin(), l = feature_lines.begin(); i != analysis_features.end() && l != feature_lines.end(); ++i, ++l) {

		float *pos = new float;
		*pos = trackview.editor().sample_to_pixel (*i - position);

		(*l).second->set (
			ArdourCanvas::Duple (*pos, 2.0),
			ArdourCanvas::Duple (*pos, y1)
			);

		(*l).second->set_data ("position", pos); // is this *modified* (drag?), if not use *i
		(*l).first = *i;

		if (l->first < first || l->first >= last) {
			l->second->hide();
		} else {
			l->second->show();
		}
	}
}

void
AudioRegionView::update_transient(float /*old_pos*/, float new_pos)
{
	/* Find sample at old pos, calulate new sample then update region transients*/
	list<std::pair<samplepos_t, ArdourCanvas::Line*> >::iterator l;

	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {

		/* Line has been updated in drag so we compare to new_pos */

		float* pos = (float*) (*l).second->get_data ("position");

		if (rint(new_pos) == rint(*pos)) {
			samplepos_t position = _region->position_sample();
			samplepos_t old_sample = (*l).first;
			samplepos_t new_sample = trackview.editor().pixel_to_sample (new_pos) + position;
			_region->update_transient (old_sample, new_sample);
			break;
		}
	}
}

void
AudioRegionView::remove_transient (float pos)
{
	/* this is called from Editor::remove_transient () with pos == get_data ("position")
	 * which is the item's x-coordinate inside the ARV.
	 *
	 * Find sample at old pos, calulate new sample then update region transients
	 */
	list<std::pair<samplepos_t, ArdourCanvas::Line*> >::iterator l;

	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {
		float *line_pos = (float*) (*l).second->get_data ("position");
		if (rint(pos) == rint(*line_pos)) {
			_region->remove_transient ((*l).first);
			break;
		}
	}
}

void
AudioRegionView::thaw_after_trim ()
{
	RegionView::thaw_after_trim ();
	unhide_envelope ();
	drag_end ();
}


void
AudioRegionView::show_xfades ()
{
	show_start_xfade ();
	show_end_xfade ();
}

void
AudioRegionView::drag_start ()
{
	TimeAxisViewItem::drag_start ();

	//we used to hide xfades here.  I don't see the point with the new model, but we can re-implement if needed
}

void
AudioRegionView::drag_end ()
{
	TimeAxisViewItem::drag_end ();
	//see comment for drag_start

	if (fade_in_handle && fade_in_handle->visible()) {
		// lenght of region or fade changed, re-check
		// if fade_in_trim_handle or fade_out_trim_handle should
		// be visible. -- If the fade_in_handle is visible
		// we have focus and are not in internal edit mode.
		entered();
	}
}

void
AudioRegionView::parameter_changed (string const & p)
{
	RegionView::parameter_changed (p);
	if (p == "show-waveforms") {
		setup_waveform_visibility ();
	}
}
