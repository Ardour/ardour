/*
    Copyright (C) 2001-2006 Paul Davis

    This program is free software; you can r>edistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>
#include <cassert>
#include <algorithm>

#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour/playlist.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"

#include "evoral/Curve.hpp"

#include "streamview.h"
#include "audio_region_view.h"
#include "audio_time_axis.h"
#include "simplerect.h"
#include "simpleline.h"
#include "waveview.h"
#include "public_editor.h"
#include "audio_region_editor.h"
#include "audio_streamview.h"
#include "region_gain_line.h"
#include "control_point.h"
#include "ghostregion.h"
#include "audio_time_axis.h"
#include "utils.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "ardour_ui.h"

#include "i18n.h"

#define MUTED_ALPHA 10

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

static const int32_t sync_mark_width = 9;
static double const handle_size = 6; /* height of fade handles */

AudioRegionView::AudioRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv, boost::shared_ptr<AudioRegion> r, double spu,
				  Gdk::Color const & basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, sync_mark(0)
	, fade_in_shape(0)
	, fade_out_shape(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, start_xfade_in (0)
	, start_xfade_out (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_in (0)
	, end_xfade_out (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis(1.0)
	, fade_color(0)
{
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&AudioRegionView::parameter_changed, this, _1), gui_context());
}

AudioRegionView::AudioRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv, boost::shared_ptr<AudioRegion> r, double spu,
				  Gdk::Color const & basic_color, bool recording, TimeAxisViewItem::Visibility visibility)
	: RegionView (parent, tv, r, spu, basic_color, recording, visibility)
	, sync_mark(0)
	, fade_in_shape(0)
	, fade_out_shape(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, start_xfade_in (0)
	, start_xfade_out (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_in (0)
	, end_xfade_out (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis(1.0)
	, fade_color(0)
{
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&AudioRegionView::parameter_changed, this, _1), gui_context());
}

AudioRegionView::AudioRegionView (const AudioRegionView& other, boost::shared_ptr<AudioRegion> other_region)
	: RegionView (other, boost::shared_ptr<Region> (other_region))
	, fade_in_shape(0)
	, fade_out_shape(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, start_xfade_in (0)
	, start_xfade_out (0)
	, start_xfade_rect (0)
	, _start_xfade_visible (false)
	, end_xfade_in (0)
	, end_xfade_out (0)
	, end_xfade_rect (0)
	, _end_xfade_visible (false)
	, _amplitude_above_axis (other._amplitude_above_axis)
	, fade_color(0)
{
	Gdk::Color c;
	int r,g,b,a;

	UINT_TO_RGBA (other.fill_color, &r, &g, &b, &a);
	c.set_rgb_p (r/255.0, g/255.0, b/255.0);

	init (c, true);

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&AudioRegionView::parameter_changed, this, _1), gui_context());
}

void
AudioRegionView::init (Gdk::Color const & basic_color, bool wfd)
{
	// FIXME: Some redundancy here with RegionView::init.  Need to figure out
	// where order is important and where it isn't...

	RegionView::init (basic_color, wfd);

	_amplitude_above_axis = 1.0;

	compute_colors (basic_color);

	create_waves ();

	fade_in_shape = new ArdourCanvas::Polygon (*group);
	fade_in_shape->property_fill_color_rgba() = fade_color;
	fade_in_shape->set_data ("regionview", this);

	fade_out_shape = new ArdourCanvas::Polygon (*group);
	fade_out_shape->property_fill_color_rgba() = fade_color;
	fade_out_shape->set_data ("regionview", this);

	if (!_recregion) {
		fade_in_handle = new ArdourCanvas::SimpleRect (*group);
		fade_in_handle->property_fill_color_rgba() = UINT_RGBA_CHANGE_A (fill_color, 0);
		fade_in_handle->property_outline_color_rgba() = RGBA_TO_UINT (0, 0, 0, 0);

		fade_in_handle->set_data ("regionview", this);

		fade_out_handle = new ArdourCanvas::SimpleRect (*group);
		fade_out_handle->property_fill_color_rgba() = UINT_RGBA_CHANGE_A (fill_color, 0);
		fade_out_handle->property_outline_color_rgba() = RGBA_TO_UINT (0, 0, 0, 0);

		fade_out_handle->set_data ("regionview", this);
	}

	setup_fade_handle_positions ();

	if (!trackview.session()->config.get_show_region_fades()) {
		set_fade_visibility (false);
	}

	const string line_name = _region->name() + ":gain";

	if (!Profile->get_sae()) {
		gain_line.reset (new AudioRegionGainLine (line_name, *this, *group, audio_region()->envelope()));
	}
	
	update_envelope_visibility ();
	gain_line->reset ();

	set_height (trackview.current_height());

	region_muted ();
	region_sync_changed ();

	region_resized (ARDOUR::bounds_change);
	set_waveview_data_src();
	region_locked ();
	envelope_active_changed ();
	fade_in_active_changed ();
	fade_out_active_changed ();

	reset_width_dependent_items (_pixel_width);

	fade_in_shape->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_in_event), fade_in_shape, this));
	if (fade_in_handle) {
		fade_in_handle->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_in_handle_event), fade_in_handle, this));
	}

	fade_out_shape->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_out_event), fade_out_shape, this));

	if (fade_out_handle) {
		fade_out_handle->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_out_handle_event), fade_out_handle, this));
	}

	set_colors ();

	setup_waveform_visibility ();
	setup_waveform_shape ();
	setup_waveform_scale ();

	/* XXX sync mark drag? */
}

AudioRegionView::~AudioRegionView ()
{
	in_destructor = true;

	RegionViewGoingAway (this); /* EMIT_SIGNAL */

	for (vector<GnomeCanvasWaveViewCache *>::iterator cache = wave_caches.begin(); cache != wave_caches.end() ; ++cache) {
		gnome_canvas_waveview_cache_destroy (*cache);
	}

	for (vector<ScopedConnection*>::iterator i = _data_ready_connections.begin(); i != _data_ready_connections.end(); ++i) {
		delete *i;
	}

	for (list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator i = feature_lines.begin(); i != feature_lines.end(); ++i) {
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
	if (what_changed.contains (ARDOUR::Properties::fade_in_is_xfade)) {
		fade_in_changed ();
	}
	if (what_changed.contains (ARDOUR::Properties::fade_out_is_xfade)) {
		fade_out_changed ();
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
	if (audio_region()->fade_in_active()) {
		fade_in_shape->property_fill_color_rgba() = RGBA_TO_UINT(45,45,45,90);				// FIXME make a themeable colour
		fade_in_shape->property_width_pixels() = 1;
	} else {
		fade_in_shape->property_fill_color_rgba() = RGBA_TO_UINT(45,45,45,20);				// FIXME make a themeable colour
		fade_in_shape->property_width_pixels() = 1;
	}
}

void
AudioRegionView::fade_out_active_changed ()
{
	if (audio_region()->fade_out_active()) {
		fade_out_shape->property_fill_color_rgba() = RGBA_TO_UINT(45,45,45,90);				// FIXME make a themeable colour
		fade_out_shape->property_width_pixels() = 1;
	} else {
		fade_out_shape->property_fill_color_rgba() = RGBA_TO_UINT(45,45,45,20);				// FIXME make a themeable colour
		fade_out_shape->property_width_pixels() = 1;
	}
}


void
AudioRegionView::region_scale_amplitude_changed ()
{
	ENSURE_GUI_THREAD (*this, &AudioRegionView::region_scale_amplitude_changed)

	for (uint32_t n = 0; n < waves.size(); ++n) {
		// force a reload of the cache
		waves[n]->property_data_src() = _region.get();
	}
}

void
AudioRegionView::region_renamed ()
{
	std::string str = RegionView::make_name ();

	if (audio_region()->speed_mismatch (trackview.session()->frame_rate())) {
		str = string ("*") + str;
	}

	if (_region->muted()) {
		str = string ("!") + str;
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
			waves[n]->property_region_start() = _region->start();
		}

		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
			if ((agr = dynamic_cast<AudioGhostRegion*>(*i)) != 0) {

				for (vector<WaveView*>::iterator w = agr->waves.begin(); w != agr->waves.end(); ++w) {
					(*w)->property_region_start() = _region->start();
				}
			}
		}

		/* hide transient lines that extend beyond the region end */

		list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

		for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {
			if (l->first > _region->length() - 1) {
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
	RegionView::reset_width_dependent_items(pixel_width);
	assert(_pixel_width == pixel_width);

	if (fade_in_handle) {
		if (pixel_width <= 6.0 || _height < 5.0 || !trackview.session()->config.get_show_region_fades()) {
			fade_in_handle->hide();
			fade_out_handle->hide();
		}
		else {
			fade_in_handle->show();
			fade_out_handle->show();
		}
	}

	AnalysisFeatureList analysis_features = _region->transients();
	AnalysisFeatureList::const_iterator i;

	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (i = analysis_features.begin(), l = feature_lines.begin(); i != analysis_features.end() && l != feature_lines.end(); ++i, ++l) {

		float x_pos = trackview.editor().frame_to_pixel (*i);

		ArdourCanvas::Points points;
		points.push_back(Gnome::Art::Point(x_pos, 2.0)); // first x-coord needs to be a non-normal value
		points.push_back(Gnome::Art::Point(x_pos, _height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 1));

		(*l).first = *i;
		(*l).second->property_points() = points;
	}

	reset_fade_shapes ();
}

void
AudioRegionView::region_muted ()
{
	RegionView::region_muted();

	for (uint32_t n=0; n < waves.size(); ++n) {
		if (_region->muted()) {
			waves[n]->property_wave_color() = UINT_RGBA_CHANGE_A(ARDOUR_UI::config()->canvasvar_WaveForm.get(), MUTED_ALPHA);
		} else {
			waves[n]->property_wave_color() = ARDOUR_UI::config()->canvasvar_WaveForm.get();
		}
	}
}

void
AudioRegionView::setup_fade_handle_positions()
{
	/* position of fade handle offset from the top of the region view */
	double const handle_pos = 2;

	if (fade_in_handle) {
		fade_in_handle->property_y1() = handle_pos;
		fade_in_handle->property_y2() = handle_pos + handle_size;
	}

	if (fade_out_handle) {
		fade_out_handle->property_y1() = handle_pos;
		fade_out_handle->property_y2() = handle_pos + handle_size;
	}
}

void
AudioRegionView::set_height (gdouble height)
{
	RegionView::set_height (height);

	uint32_t wcnt = waves.size();

	for (uint32_t n = 0; n < wcnt; ++n) {
		gdouble ht;

		if (height < NAME_HIGHLIGHT_THRESH) {
			ht = ((height - 2 * wcnt) / (double) wcnt);
		} else {
			ht = (((height - 2 * wcnt) - NAME_HIGHLIGHT_SIZE) / (double) wcnt);
		}

		gdouble yoff = n * (ht + 1);

		waves[n]->property_height() = ht;
		waves[n]->property_y() = yoff + 2;
	}

	if (gain_line) {

		if ((height/wcnt) < NAME_HIGHLIGHT_THRESH) {
			gain_line->hide ();
		} else {
			update_envelope_visibility ();
		}

		gain_line->set_height ((uint32_t) rint (height - NAME_HIGHLIGHT_SIZE) - 2);
	}

	reset_fade_shapes ();

	/* Update hights for any active feature lines */
	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {

		float pos_x = trackview.editor().frame_to_pixel((*l).first);

		ArdourCanvas::Points points;

		points.push_back(Gnome::Art::Point(pos_x, 2.0)); // first x-coord needs to be a non-normal value
		points.push_back(Gnome::Art::Point(pos_x, _height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 1));

		(*l).second->property_points() = points;
	}

	if (name_pixbuf) {
		name_pixbuf->raise_to_top();
	}
}

void
AudioRegionView::reset_fade_shapes ()
{
	reset_fade_in_shape ();
	reset_fade_out_shape ();
}

void
AudioRegionView::reset_fade_in_shape ()
{
	reset_fade_in_shape_width (audio_region(), (framecnt_t) audio_region()->fade_in()->back()->when);
}

void
AudioRegionView::reset_fade_in_shape_width (boost::shared_ptr<AudioRegion> ar, framecnt_t width)
{
	if (dragging()) {
		return;
	}

	if (audio_region()->fade_in_is_xfade()) {
		if (fade_in_handle) {
			fade_in_handle->hide ();
			fade_in_shape->hide ();
		}
		redraw_start_xfade ();
		return;
	} else {
		if (start_xfade_in) {
			start_xfade_in->hide ();
			start_xfade_out->hide ();
			start_xfade_rect->hide ();
			_start_xfade_visible = false;
		}
	}

	if (fade_in_handle == 0) {
		return;
	}

	fade_in_handle->show ();

	/* smallest size for a fade is 64 frames */

	width = std::max ((framecnt_t) 64, width);

	Points* points;

	/* round here to prevent little visual glitches with sub-pixel placement */
	double const pwidth = rint (width / samples_per_unit);
	uint32_t npoints = std::min (gdk_screen_width(), (int) pwidth);
	double h;

	double const handle_center = pwidth;

	/* Put the fade in handle so that its left side is at the end-of-fade line */
	fade_in_handle->property_x1() = handle_center;
	fade_in_handle->property_x2() = handle_center + handle_size;

	if (pwidth < 5) {
		hide_start_xfade();
		fade_in_shape->hide();
		return;
	}

	if (trackview.session()->config.get_show_region_fades()) {
		fade_in_shape->show();
	}

	float curve[npoints];
	audio_region()->fade_in()->curve().get_vector (0, audio_region()->fade_in()->back()->when, curve, npoints);

	points = get_canvas_points ("fade in shape", npoints + 3);

	if (_height >= NAME_HIGHLIGHT_THRESH) {
		h = _height - NAME_HIGHLIGHT_SIZE - 2;
	} else {
		h = _height - 2;
	}

	/* points *MUST* be in anti-clockwise order */

	uint32_t pi, pc;
	double xdelta = pwidth/npoints;

	for (pi = 0, pc = 0; pc < npoints; ++pc) {
		(*points)[pi].set_x(1 + (pc * xdelta));
		(*points)[pi++].set_y(2 + (h - (curve[pc] * h)));
	}

	/* fold back */

	(*points)[pi].set_x(pwidth);
	(*points)[pi++].set_y(2);

	(*points)[pi].set_x(1);
	(*points)[pi++].set_y(2);

	/* connect the dots ... */

	(*points)[pi] = (*points)[0];

	fade_in_shape->property_points() = *points;
	delete points;

	redraw_start_xfade_to ( ar, width);

	/* ensure trim handle stays on top */
	if (frame_handle_start) {
		frame_handle_start->raise_to_top();
	}

}

void
AudioRegionView::reset_fade_out_shape ()
{
	reset_fade_out_shape_width (audio_region(), (framecnt_t) audio_region()->fade_out()->back()->when);
}

void
AudioRegionView::reset_fade_out_shape_width (boost::shared_ptr<AudioRegion> ar, framecnt_t width)
{
	if (dragging() && audio_region()->fade_out_is_xfade()) {
		/* we hide xfades while dragging regions */
		return;
	}

	if (audio_region()->fade_out_is_xfade()) {
		if (fade_out_handle) {
			fade_out_handle->hide ();
			fade_out_shape->hide ();
		}
		redraw_end_xfade ();
		return;
	} else {
		if (end_xfade_in) {
			end_xfade_in->hide ();
			end_xfade_out->hide ();
			end_xfade_rect->hide ();
			_end_xfade_visible = false;
		}
	}

	if (fade_out_handle == 0) {
		return;
	}

	fade_out_handle->show ();

	/* smallest size for a fade is 64 frames */

	width = std::max ((framecnt_t) 64, width);

	Points* points;

	/* round here to prevent little visual glitches with sub-pixel placement */
	double const pwidth = rint (width / samples_per_unit);
	uint32_t npoints = std::min (gdk_screen_width(), (int) pwidth);
	double h;

	double const handle_center = (_region->length() - width) / samples_per_unit;

	/* Put the fade out handle so that its right side is at the end-of-fade line;
	 * it's `one out' for precise pixel accuracy.
	 */
	fade_out_handle->property_x1() = handle_center - 5;
	fade_out_handle->property_x2() = handle_center + 1;

	/* don't show shape if its too small */

	if (pwidth < 5) {
		hide_end_xfade();
		fade_out_shape->hide();
		return;
	}

	if (trackview.session()->config.get_show_region_fades()) {
		fade_out_shape->show();
	}

	float curve[npoints];
	audio_region()->fade_out()->curve().get_vector (0, audio_region()->fade_out()->back()->when, curve, npoints);

	if (_height >= NAME_HIGHLIGHT_THRESH) {
		h = _height - NAME_HIGHLIGHT_SIZE - 2;
	} else {
		h = _height - 2;
	}

	/* points *MUST* be in anti-clockwise order */

	points = get_canvas_points ("fade out shape", npoints + 3);

	uint32_t pi, pc;
	double xdelta = pwidth/npoints;

	for (pi = 0, pc = 0; pc < npoints; ++pc) {
		(*points)[pi].set_x(_pixel_width - pwidth + (pc * xdelta));
		(*points)[pi++].set_y(2 + (h - (curve[pc] * h)));
	}

	/* fold back */

	(*points)[pi].set_x(_pixel_width);
	(*points)[pi++].set_y(h);

	(*points)[pi].set_x(_pixel_width);
	(*points)[pi++].set_y(2);

	/* connect the dots ... */

	(*points)[pi] = (*points)[0];

	fade_out_shape->property_points() = *points;
	delete points;

	redraw_end_xfade_to (ar, width);

	/* ensure trim handle stays on top */
	if (frame_handle_end) {
		frame_handle_end->raise_to_top();
	}

}

framepos_t
AudioRegionView::get_fade_in_shape_width ()
{
	return audio_region()->fade_in()->back()->when;
}

framepos_t
AudioRegionView::get_fade_out_shape_width ()
{
	return audio_region()->fade_out()->back()->when;
}


void
AudioRegionView::set_samples_per_unit (gdouble spu)
{
	RegionView::set_samples_per_unit (spu);

	if (Config->get_show_waveforms ()) {
		for (uint32_t n = 0; n < waves.size(); ++n) {
			waves[n]->property_samples_per_unit() = spu;
		}
	}

	if (gain_line) {
		gain_line->reset ();
	}

	reset_fade_shapes ();
}

void
AudioRegionView::set_amplitude_above_axis (gdouble spp)
{
	for (uint32_t n=0; n < waves.size(); ++n) {
		waves[n]->property_amplitude_above_axis() = spp;
	}
}

void
AudioRegionView::compute_colors (Gdk::Color const & basic_color)
{
	RegionView::compute_colors (basic_color);

	/* gain color computed in envelope_active_changed() */

	fade_color = UINT_RGBA_CHANGE_A (fill_color, 120);
}

void
AudioRegionView::set_colors ()
{
	RegionView::set_colors();

	if (gain_line) {
		gain_line->set_line_color (audio_region()->envelope_active() ? ARDOUR_UI::config()->canvasvar_GainLine.get() : ARDOUR_UI::config()->canvasvar_GainLineInactive.get());
	}

	for (uint32_t n=0; n < waves.size(); ++n) {
		if (_region->muted()) {
			waves[n]->property_wave_color() = UINT_RGBA_CHANGE_A(ARDOUR_UI::config()->canvasvar_WaveForm.get(), MUTED_ALPHA);
		} else {
			waves[n]->property_wave_color() = ARDOUR_UI::config()->canvasvar_WaveForm.get();
		}

		waves[n]->property_clip_color() = ARDOUR_UI::config()->canvasvar_WaveFormClip.get();
		waves[n]->property_zero_color() = ARDOUR_UI::config()->canvasvar_ZeroLine.get();
	}
}

void
AudioRegionView::setup_waveform_visibility ()
{
	if (Config->get_show_waveforms ()) {
		for (uint32_t n = 0; n < waves.size(); ++n) {
			/* make sure the zoom level is correct, since we don't update
			   this when waveforms are hidden.
			*/
			waves[n]->property_samples_per_unit() = samples_per_unit;
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

	if (Config->get_show_region_gain() || trackview.editor().current_mouse_mode() == Editing::MouseGain) {
		gain_line->add_visibility (AutomationLine::Line);
	} else {
		gain_line->hide ();
	}
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

		wave_caches.push_back (WaveView::create_cache ());

		// cerr << "\tchannel " << n << endl;

		if (wait_for_data) {
			if (audio_region()->audio_source(n)->peaks_ready (boost::bind (&AudioRegionView::peaks_ready_handler, this, n), &_data_ready_connections[n], gui_context())) {
				// cerr << "\tData is ready\n";
				create_one_wave (n, true);
			} else {
				// cerr << "\tdata is not ready\n";
				// we'll get a PeaksReady signal from the source in the future
				// and will call create_one_wave(n) then.
			}

		} else {
			// cerr << "\tdon't delay, display today!\n";
			create_one_wave (n, true);
		}

	}
}

void
AudioRegionView::create_one_wave (uint32_t which, bool /*direct*/)
{
	//cerr << "AudioRegionView::create_one_wave() called which: " << which << " this: " << this << endl;//DEBUG
	RouteTimeAxisView& atv (*(dynamic_cast<RouteTimeAxisView*>(&trackview))); // ick
	uint32_t nchans = atv.track()->n_channels().n_audio();
	uint32_t n;
	uint32_t nwaves = std::min (nchans, audio_region()->n_channels());
	gdouble ht;

	if (trackview.current_height() < NAME_HIGHLIGHT_THRESH) {
		ht = ((trackview.current_height()) / (double) nchans);
	} else {
		ht = ((trackview.current_height() - NAME_HIGHLIGHT_SIZE) / (double) nchans);
	}

	gdouble yoff = which * ht;

	WaveView *wave = new WaveView(*group);

	wave->property_data_src() = (gpointer) _region.get();
	wave->property_cache() =  wave_caches[which];
	wave->property_cache_updater() = true;
	wave->property_channel() =  which;
	wave->property_length_function() = (gpointer) region_length_from_c;
	wave->property_sourcefile_length_function() = (gpointer) sourcefile_length_from_c;
	wave->property_peak_function() =  (gpointer) region_read_peaks_from_c;
	wave->property_x() =  0.0;
	wave->property_y() =  yoff;
	wave->property_height() =  (double) ht;
	wave->property_samples_per_unit() =  samples_per_unit;
	wave->property_amplitude_above_axis() =  _amplitude_above_axis;

	if (_recregion) {
		wave->property_wave_color() = _region->muted() ? UINT_RGBA_CHANGE_A(ARDOUR_UI::config()->canvasvar_RecWaveForm.get(), MUTED_ALPHA) : ARDOUR_UI::config()->canvasvar_RecWaveForm.get();
		wave->property_fill_color() = ARDOUR_UI::config()->canvasvar_RecWaveFormFill.get();
	} else {
		wave->property_wave_color() = _region->muted() ? UINT_RGBA_CHANGE_A(ARDOUR_UI::config()->canvasvar_WaveForm.get(), MUTED_ALPHA) : ARDOUR_UI::config()->canvasvar_WaveForm.get();
		wave->property_fill_color() = ARDOUR_UI::config()->canvasvar_WaveFormFill.get();
	}

	wave->property_clip_color() = ARDOUR_UI::config()->canvasvar_WaveFormClip.get();
	wave->property_zero_color() = ARDOUR_UI::config()->canvasvar_ZeroLine.get();
	wave->property_zero_line() = true;
	wave->property_region_start() = _region->start();
	wave->property_rectified() = Config->get_waveform_shape() == Rectified;
	wave->property_logscaled() = Config->get_waveform_scale() == Logarithmic;

	if (!Config->get_show_waveforms ()) {
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

	if (n == nwaves && waves.empty()) {
		/* all waves are ready */
		tmp_waves.resize(nwaves);

		waves = tmp_waves;
		tmp_waves.clear ();

		/* all waves created, don't hook into peaks ready anymore */
		delete _data_ready_connections[which];
		_data_ready_connections[which] = 0;
	}
}

void
AudioRegionView::peaks_ready_handler (uint32_t which)
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&AudioRegionView::create_one_wave, this, which, false));
	// cerr << "AudioRegionView::peaks_ready_handler() called on " << which << " this: " << this << endl;
}

void
AudioRegionView::add_gain_point_event (ArdourCanvas::Item *item, GdkEvent *ev)
{
	if (!gain_line) {
		return;
	}

	double x, y;

	/* don't create points that can't be seen */

	update_envelope_visibility ();

	x = ev->button.x;
	y = ev->button.y;

	item->w2i (x, y);

	framepos_t fx = trackview.editor().pixel_to_frame (x);

	if (fx > _region->length()) {
		return;
	}

	/* compute vertical fractional position */

	y = 1.0 - (y / (_height - NAME_HIGHLIGHT_SIZE));

	/* map using gain line */

	gain_line->view_to_model_coord (x, y);

	/* XXX STATEFUL: can't convert to stateful diff until we
	   can represent automation data with it.
	*/

	trackview.session()->begin_reversible_command (_("add gain control point"));
	XMLNode &before = audio_region()->envelope()->get_state();

	if (!audio_region()->envelope_active()) {
		XMLNode &region_before = audio_region()->get_state();
		audio_region()->set_envelope_active(true);
		XMLNode &region_after = audio_region()->get_state();
		trackview.session()->add_command (new MementoCommand<AudioRegion>(*(audio_region().get()), &region_before, &region_after));
	}

	audio_region()->envelope()->add (fx, y);

	XMLNode &after = audio_region()->envelope()->get_state();
	trackview.session()->add_command (new MementoCommand<AutomationList>(*audio_region()->envelope().get(), &before, &after));
	trackview.session()->commit_reversible_command ();
}

void
AudioRegionView::remove_gain_point_event (ArdourCanvas::Item *item, GdkEvent */*ev*/)
{
	ControlPoint *cp = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"));
	audio_region()->envelope()->erase (cp->model());
}

void
AudioRegionView::setup_waveform_shape ()
{
	for (vector<WaveView *>::iterator wave = waves.begin(); wave != waves.end() ; ++wave) {
		(*wave)->property_rectified() = Config->get_waveform_shape() == Rectified;
	}
}

void
AudioRegionView::setup_waveform_scale ()
{
	for (vector<WaveView *>::iterator wave = waves.begin(); wave != waves.end() ; ++wave) {
		(*wave)->property_logscaled() = Config->get_waveform_scale() == Logarithmic;
	}
}


GhostRegion*
AudioRegionView::add_ghost (TimeAxisView& tv)
{
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&trackview);
	assert(rtv);

	double unit_position = _region->position () / samples_per_unit;
	AudioGhostRegion* ghost = new AudioGhostRegion (tv, trackview, unit_position);
	uint32_t nchans;

	nchans = rtv->track()->n_channels().n_audio();

	for (uint32_t n = 0; n < nchans; ++n) {

		if (n >= audio_region()->n_channels()) {
			break;
		}

		WaveView *wave = new WaveView(*ghost->group);

		wave->property_data_src() = _region.get();
		wave->property_cache() =  wave_caches[n];
		wave->property_cache_updater() = false;
		wave->property_channel() = n;
		wave->property_length_function() = (gpointer)region_length_from_c;
		wave->property_sourcefile_length_function() = (gpointer) sourcefile_length_from_c;
		wave->property_peak_function() =  (gpointer) region_read_peaks_from_c;
		wave->property_x() =  0.0;
		wave->property_samples_per_unit() =  samples_per_unit;
		wave->property_amplitude_above_axis() =  _amplitude_above_axis;

		wave->property_region_start() = _region->start();

		ghost->waves.push_back(wave);
	}

	ghost->set_height ();
	ghost->set_duration (_region->length() / samples_per_unit);
	ghost->set_colors();
	ghosts.push_back (ghost);

	return ghost;
}

void
AudioRegionView::entered (bool internal_editing)
{
	trackview.editor().set_current_trimmable (_region);
	trackview.editor().set_current_movable (_region);
	
	if (gain_line && trackview.editor().current_mouse_mode() == Editing::MouseGain) {
		gain_line->add_visibility (AutomationLine::ControlPoints);
	}

	if (fade_in_handle && !internal_editing) {
		fade_in_handle->property_outline_color_rgba() = RGBA_TO_UINT (0, 0, 0, 255);
		fade_in_handle->property_fill_color_rgba() = UINT_RGBA_CHANGE_A (fade_color, 255);
		fade_out_handle->property_outline_color_rgba() = RGBA_TO_UINT (0, 0, 0, 255);
		fade_out_handle->property_fill_color_rgba() = UINT_RGBA_CHANGE_A (fade_color, 255);
	}
}

void
AudioRegionView::exited ()
{
	trackview.editor().set_current_trimmable (boost::shared_ptr<Trimmable>());
	trackview.editor().set_current_movable (boost::shared_ptr<Movable>());

	if (gain_line && trackview.editor().current_mouse_mode() == Editing::MouseGain) {
		gain_line->remove_visibility (AutomationLine::ControlPoints);
	}

	if (fade_in_handle) {
		fade_in_handle->property_outline_color_rgba() = RGBA_TO_UINT (0, 0, 0, 0);
		fade_in_handle->property_fill_color_rgba() = UINT_RGBA_CHANGE_A (fade_color, 0);
		fade_out_handle->property_outline_color_rgba() = RGBA_TO_UINT (0, 0, 0, 0);
		fade_out_handle->property_fill_color_rgba() = UINT_RGBA_CHANGE_A (fade_color, 0);
	}
}

void
AudioRegionView::envelope_active_changed ()
{
	if (gain_line) {
		gain_line->set_line_color (audio_region()->envelope_active() ? ARDOUR_UI::config()->canvasvar_GainLine.get() : ARDOUR_UI::config()->canvasvar_GainLineInactive.get());
	}
}

void
AudioRegionView::set_waveview_data_src()
{
	AudioGhostRegion* agr;
	double unit_length= _region->length() / samples_per_unit;

	for (uint32_t n = 0; n < waves.size(); ++n) {
		// TODO: something else to let it know the channel
		waves[n]->property_data_src() = _region.get();
	}

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {

		(*i)->set_duration (unit_length);

		if((agr = dynamic_cast<AudioGhostRegion*>(*i)) != 0) {
			for (vector<WaveView*>::iterator w = agr->waves.begin(); w != agr->waves.end(); ++w) {
				(*w)->property_data_src() = _region.get();
			}
		}
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
AudioRegionView::set_frame_color ()
{
	if (!frame) {
		return;
	}

	if (_region->opaque()) {
		fill_opacity = 130;
	} else {
		fill_opacity = 0;
	}

	TimeAxisViewItem::set_frame_color ();

        uint32_t wc;
        uint32_t fc;

	if (_selected) {
                if (_region->muted()) {
                        wc = UINT_RGBA_CHANGE_A(ARDOUR_UI::config()->canvasvar_SelectedWaveForm.get(), MUTED_ALPHA);
                } else {
                        wc = ARDOUR_UI::config()->canvasvar_SelectedWaveForm.get();
                }
                fc = ARDOUR_UI::config()->canvasvar_SelectedWaveFormFill.get();
	} else {
		if (_recregion) {
                        if (_region->muted()) {
                                wc = UINT_RGBA_CHANGE_A(ARDOUR_UI::config()->canvasvar_RecWaveForm.get(), MUTED_ALPHA);
                        } else {
                                wc = ARDOUR_UI::config()->canvasvar_RecWaveForm.get();
                        }
                        fc = ARDOUR_UI::config()->canvasvar_RecWaveFormFill.get();
		} else {
                        if (_region->muted()) {
                                wc = UINT_RGBA_CHANGE_A(ARDOUR_UI::config()->canvasvar_WaveForm.get(), MUTED_ALPHA);
                        } else {
                                wc = ARDOUR_UI::config()->canvasvar_WaveForm.get();
                        }
                        fc = ARDOUR_UI::config()->canvasvar_WaveFormFill.get();
		}
	}

        for (vector<ArdourCanvas::WaveView*>::iterator w = waves.begin(); w != waves.end(); ++w) {
                if (_region->muted()) {
                        (*w)->property_wave_color() = wc;
                } else {
                        (*w)->property_wave_color() = wc;
                        (*w)->property_fill_color() = fc;
                }
        }
}

void
AudioRegionView::set_fade_visibility (bool yn)
{
	if (yn) {
		if (fade_in_shape) {
			fade_in_shape->show();
		}
		if (fade_out_shape) {
			fade_out_shape->show ();
		}
		if (fade_in_handle) {
			fade_in_handle->show ();
		}
		if (fade_out_handle) {
			fade_out_handle->show ();
		}
	} else {
		if (fade_in_shape) {
			fade_in_shape->hide();
		}
		if (fade_out_shape) {
			fade_out_shape->hide ();
		}
		if (fade_in_handle) {
			fade_in_handle->hide ();
		}
		if (fade_out_handle) {
			fade_out_handle->hide ();
		}
	}
}

void
AudioRegionView::update_coverage_frames (LayerDisplay d)
{
	RegionView::update_coverage_frames (d);

	if (fade_in_handle) {
		fade_in_handle->raise_to_top ();
		fade_out_handle->raise_to_top ();
	}
}

void
AudioRegionView::show_region_editor ()
{
	if (editor == 0) {
		editor = new AudioRegionEditor (trackview.session(), audio_region());
	}

	editor->present ();
	editor->set_position (Gtk::WIN_POS_MOUSE);
	editor->show_all();
}

void
AudioRegionView::transients_changed ()
{
	AnalysisFeatureList analysis_features = _region->transients();

	while (feature_lines.size() < analysis_features.size()) {

		ArdourCanvas::Line* canvas_item = new ArdourCanvas::Line(*group);

		ArdourCanvas::Points points;

		points.push_back(Gnome::Art::Point(-1.0, 2.0)); // first x-coord needs to be a non-normal value
		points.push_back(Gnome::Art::Point(1.0, _height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 1));

		canvas_item->property_points() = points;
		canvas_item->property_width_pixels() = 1;
		canvas_item->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_ZeroLine.get();
		canvas_item->property_first_arrowhead() = TRUE;
		canvas_item->property_last_arrowhead() = TRUE;
		canvas_item->property_arrow_shape_a() = 11.0;
		canvas_item->property_arrow_shape_b() = 0.0;
		canvas_item->property_arrow_shape_c() = 4.0;

		canvas_item->raise_to_top ();
		canvas_item->show ();

		canvas_item->set_data ("regionview", this);
		canvas_item->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_feature_line_event), canvas_item, this));

		feature_lines.push_back (make_pair(0, canvas_item));
	}

	while (feature_lines.size() > analysis_features.size()) {
		ArdourCanvas::Line* line = feature_lines.back().second;
		feature_lines.pop_back ();
		delete line;
	}

	AnalysisFeatureList::const_iterator i;
	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (i = analysis_features.begin(), l = feature_lines.begin(); i != analysis_features.end() && l != feature_lines.end(); ++i, ++l) {

		ArdourCanvas::Points points;

		float *pos = new float;
		*pos = trackview.editor().frame_to_pixel (*i);

		points.push_back(Gnome::Art::Point(*pos, 2.0)); // first x-coord needs to be a non-normal value
		points.push_back(Gnome::Art::Point(*pos, _height - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 1));

		(*l).second->property_points() = points;
		(*l).second->set_data ("position", pos);

		(*l).first = *i;
	}
}

void
AudioRegionView::update_transient(float /*old_pos*/, float new_pos)
{
	/* Find frame at old pos, calulate new frame then update region transients*/
	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {

		/* Line has been updated in drag so we compare to new_pos */

		float* pos = (float*) (*l).second->get_data ("position");

		if (rint(new_pos) == rint(*pos)) {

		    framepos_t old_frame = (*l).first;
		    framepos_t new_frame = trackview.editor().pixel_to_frame (new_pos);

		    _region->update_transient (old_frame, new_frame);

		    break;
		}
	}
}

void
AudioRegionView::remove_transient(float pos)
{
	/* Find frame at old pos, calulate new frame then update region transients*/
	list<std::pair<framepos_t, ArdourCanvas::Line*> >::iterator l;

	for (l = feature_lines.begin(); l != feature_lines.end(); ++l) {

		/* Line has been updated in drag so we compare to new_pos */
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
AudioRegionView::redraw_start_xfade ()
{
	boost::shared_ptr<AudioRegion> ar (audio_region());

	if (!ar->fade_in() || ar->fade_in()->empty()) {
		return;
	}

	show_start_xfade();

	redraw_start_xfade_to (ar, ar->fade_in()->back()->when);
}

void
AudioRegionView::redraw_start_xfade_to (boost::shared_ptr<AudioRegion> ar, framecnt_t len)
{
	int32_t const npoints = trackview.editor().frame_to_pixel (len);

	if (npoints < 3) {
		return;
	}

	if (!start_xfade_in) {
		start_xfade_in = new ArdourCanvas::Line (*group);
		start_xfade_in->property_width_pixels() = 1;
		start_xfade_in->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_GainLine.get();
	}

	if (!start_xfade_out) {
		start_xfade_out = new ArdourCanvas::Line (*group);
		start_xfade_out->property_width_pixels() = 1;
		uint32_t col = UINT_RGBA_CHANGE_A (ARDOUR_UI::config()->canvasvar_GainLine.get(), 128);
		start_xfade_out->property_fill_color_rgba() = col;
	}

	if (!start_xfade_rect) {
		start_xfade_rect = new ArdourCanvas::SimpleRect (*group);
		start_xfade_rect->property_draw() = true;
		start_xfade_rect->property_fill() = true;;
		start_xfade_rect->property_fill_color_rgba() =  ARDOUR_UI::config()->canvasvar_ActiveCrossfade.get();
		start_xfade_rect->property_outline_pixels() = 0;
		start_xfade_rect->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_start_xfade_event), start_xfade_rect, this));
		start_xfade_rect->set_data ("regionview", this);
	}

	Points* points = get_canvas_points ("xfade edit redraw", npoints);
	boost::scoped_array<float> vec (new float[npoints]);

	double effective_height;
	if (_height >= NAME_HIGHLIGHT_THRESH) {
		effective_height = _height - NAME_HIGHLIGHT_SIZE - 2;
	} else {
		effective_height = _height - 2;
	}

	ar->fade_in()->curve().get_vector (0, ar->fade_in()->back()->when, vec.get(), npoints);

	for (int i = 0, pci = 0; i < npoints; ++i) {
		Gnome::Art::Point &p ((*points)[pci++]);
		p.set_x (i);
		p.set_y (1.0 + effective_height - (effective_height * vec.get()[i]));
	}

	start_xfade_rect->property_x1() = ((*points)[0]).get_x();
	start_xfade_rect->property_y1() = 1.0;
	start_xfade_rect->property_x2() = ((*points)[npoints-1]).get_x();
	start_xfade_rect->property_y2() = effective_height;
	start_xfade_rect->show ();

	start_xfade_in->property_points() = *points;
	start_xfade_in->show ();
	start_xfade_in->raise_to_top ();

	/* fade out line */

	boost::shared_ptr<AutomationList> inverse = ar->inverse_fade_in();

	if (!inverse) {

		for (int i = 0, pci = 0; i < npoints; ++i) {
			Gnome::Art::Point &p ((*points)[pci++]);
			p.set_x (i);
			p.set_y (1.0 + effective_height - (effective_height * (1.0 - vec.get()[i])));
		}

	} else {

		inverse->curve().get_vector (0, inverse->back()->when, vec.get(), npoints);

		for (int i = 0, pci = 0; i < npoints; ++i) {
			Gnome::Art::Point &p ((*points)[pci++]);
			p.set_x (i);
			p.set_y (1.0 + effective_height - (effective_height * vec.get()[i]));
		}
	}

	start_xfade_out->property_points() = *points;
	start_xfade_out->show ();
	start_xfade_out->raise_to_top ();

	start_xfade_rect->raise_to_top ();  //this needs to be topmost so the lines don't steal mouse focus

	show_start_xfade();

	delete points;
}

void
AudioRegionView::redraw_end_xfade ()
{
	boost::shared_ptr<AudioRegion> ar (audio_region());

	if (!ar->fade_out() || ar->fade_out()->empty()) {
		return;
	}

	show_end_xfade();

	redraw_end_xfade_to (ar, ar->fade_out()->back()->when);
}

void
AudioRegionView::redraw_end_xfade_to (boost::shared_ptr<AudioRegion> ar, framecnt_t len)
{
	int32_t const npoints = trackview.editor().frame_to_pixel (len);

	if (npoints < 3) {
		return;
	}

	if (!end_xfade_in) {
		end_xfade_in = new ArdourCanvas::Line (*group);
		end_xfade_in->property_width_pixels() = 1;
		end_xfade_in->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_GainLine.get();
	}

	if (!end_xfade_out) {
		end_xfade_out = new ArdourCanvas::Line (*group);
		end_xfade_out->property_width_pixels() = 1;
		uint32_t col UINT_RGBA_CHANGE_A (ARDOUR_UI::config()->canvasvar_GainLine.get(), 128);
		end_xfade_out->property_fill_color_rgba() = col;
	}

	if (!end_xfade_rect) {
		end_xfade_rect = new ArdourCanvas::SimpleRect (*group);
		end_xfade_rect->property_draw() = true;
		end_xfade_rect->property_fill() = true;;
		end_xfade_rect->property_fill_color_rgba() =  ARDOUR_UI::config()->canvasvar_ActiveCrossfade.get();
		end_xfade_rect->property_outline_pixels() = 0;
		end_xfade_rect->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_end_xfade_event), end_xfade_rect, this));
		end_xfade_rect->set_data ("regionview", this);
	}

	Points* points = get_canvas_points ("xfade edit redraw", npoints);
	boost::scoped_ptr<float> vec (new float[npoints]);

	ar->fade_out()->curve().get_vector (0, ar->fade_out()->back()->when, vec.get(), npoints);

	double rend = trackview.editor().frame_to_pixel (_region->length() - len);

	double effective_height;
	if (_height >= NAME_HIGHLIGHT_THRESH) {
		effective_height = _height - NAME_HIGHLIGHT_SIZE - 2;
	} else {
		effective_height = _height - 2;
	}

	for (int i = 0, pci = 0; i < npoints; ++i) {
		Gnome::Art::Point &p ((*points)[pci++]);
		p.set_x (rend + i);
		p.set_y (1.0 + effective_height - (effective_height * vec.get()[i]));
	}

	end_xfade_rect->property_x1() = ((*points)[0]).get_x();
	end_xfade_rect->property_y1() = 1;
	end_xfade_rect->property_x2() = ((*points)[npoints-1]).get_x();
	end_xfade_rect->property_y2() = effective_height;
	end_xfade_rect->show ();

	end_xfade_in->property_points() = *points;
	end_xfade_in->show ();
	end_xfade_in->raise_to_top ();

	/* fade in line */

	boost::shared_ptr<AutomationList> inverse = ar->inverse_fade_out ();

	if (!inverse) {

		for (int i = 0, pci = 0; i < npoints; ++i) {
			Gnome::Art::Point &p ((*points)[pci++]);
			p.set_x (rend + i);
			p.set_y (1.0 + effective_height - (effective_height * (1.0 - vec.get()[i])));
		}

	} else {

		inverse->curve().get_vector (inverse->front()->when, inverse->back()->when, vec.get(), npoints);

		for (int i = 0, pci = 0; i < npoints; ++i) {
			Gnome::Art::Point &p ((*points)[pci++]);
			p.set_x (rend + i);
			p.set_y (1.0 + effective_height - (effective_height * vec.get()[i]));
		}
	}

	end_xfade_out->property_points() = *points;
	end_xfade_out->show ();
	end_xfade_out->raise_to_top ();

	end_xfade_rect->raise_to_top ();  //this needs to be topmost so the lines don't steal mouse focus

	show_end_xfade();

	delete points;
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
	if (start_xfade_in) {
		start_xfade_in->hide();
	}
	if (start_xfade_out) {
		start_xfade_out->hide();
	}
	if (start_xfade_rect) {
		start_xfade_rect->hide ();
	}

	_start_xfade_visible = false;
}

void
AudioRegionView::hide_end_xfade ()
{
	if (end_xfade_in) {
		end_xfade_in->hide();
	}
	if (end_xfade_out) {
		end_xfade_out->hide();
	}
	if (end_xfade_rect) {
		end_xfade_rect->hide ();
	}

	_end_xfade_visible = false;
}

void
AudioRegionView::show_start_xfade ()
{
	if (start_xfade_in) {
		start_xfade_in->show();
	}
	if (start_xfade_out) {
		start_xfade_out->show();
	}
	if (start_xfade_rect) {
		start_xfade_rect->show ();
	}

	_start_xfade_visible = true;
}

void
AudioRegionView::show_end_xfade ()
{
	if (end_xfade_in) {
		end_xfade_in->show();
	}
	if (end_xfade_out) {
		end_xfade_out->show();
	}
	if (end_xfade_rect) {
		end_xfade_rect->show ();
	}

	_end_xfade_visible = true;
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
}

void
AudioRegionView::parameter_changed (string const & p)
{
	if (p == "show-waveforms") {
		setup_waveform_visibility ();
	} else if (p == "waveform-scale") {
		setup_waveform_scale ();
	} else if (p == "waveform-shape") {
		setup_waveform_shape ();
	}
}
