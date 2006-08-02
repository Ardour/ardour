/*
    Copyright (C) 2001-2006 Paul Davis 

    This program is free software; you can redistribute it and/or modify
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

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include <ardour/playlist.h>
#include <ardour/audioregion.h>
#include <ardour/audiosource.h>
#include <ardour/audio_diskstream.h>

#include "streamview.h"
#include "audio_region_view.h"
#include "audio_time_axis.h"
#include "simplerect.h"
#include "simpleline.h"
#include "waveview.h"
#include "public_editor.h"
#include "audio_region_editor.h"
#include "region_gain_line.h"
#include "ghostregion.h"
#include "audio_time_axis.h"
#include "utils.h"
#include "rgb_macros.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace ArdourCanvas;

static const int32_t sync_mark_width = 9;

AudioRegionView::AudioRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv, AudioRegion& r, double spu,
				  Gdk::Color& basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, sync_mark(0)
	, zero_line(0)
	, fade_in_shape(0)
	, fade_out_shape(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, gain_line(0)
	, _amplitude_above_axis(1.0)
	, _flags(0)
	, fade_color(0)
{
}

AudioRegionView::AudioRegionView (ArdourCanvas::Group *parent, RouteTimeAxisView &tv, AudioRegion& r, double spu, 
				  Gdk::Color& basic_color, TimeAxisViewItem::Visibility visibility)
	: RegionView (parent, tv, r, spu, basic_color, visibility)
	, sync_mark(0)
	, zero_line(0)
	, fade_in_shape(0)
	, fade_out_shape(0)
	, fade_in_handle(0)
	, fade_out_handle(0)
	, gain_line(0)
	, _amplitude_above_axis(1.0)
	, _flags(0)
	, fade_color(0)
{
}

void
AudioRegionView::init (Gdk::Color& basic_color, bool wfd)
{
	// FIXME: Some redundancy here with RegionView::init.  Need to figure out
	// where order is important and where it isn't...
	
	RegionView::init(basic_color, wfd);

	XMLNode *node;

	_amplitude_above_axis = 1.0;
	zero_line             = 0;
	_flags                = 0;

	if ((node = _region.extra_xml ("GUI")) != 0) {
		set_flags (node);
	} else {
		_flags = WaveformVisible;
		store_flags ();
	}

	if (trackview.editor.new_regionviews_display_gain()) {
		_flags |= EnvelopeVisible;
	}

	compute_colors (basic_color);

	create_waves ();

	fade_in_shape = new ArdourCanvas::Polygon (*group);
	fade_in_shape->property_fill_color_rgba() = fade_color;
	fade_in_shape->set_data ("regionview", this);
	
	fade_out_shape = new ArdourCanvas::Polygon (*group);
	fade_out_shape->property_fill_color_rgba() = fade_color;
	fade_out_shape->set_data ("regionview", this);


	{
		uint32_t r,g,b,a;
		UINT_TO_RGBA(fill_color,&r,&g,&b,&a);
	

		fade_in_handle = new ArdourCanvas::SimpleRect (*group);
		fade_in_handle->property_fill_color_rgba() = RGBA_TO_UINT(r,g,b,0);
		fade_in_handle->property_outline_pixels() = 0;
		fade_in_handle->property_y1() = 2.0;
		fade_in_handle->property_y2() = 7.0;
		
		fade_in_handle->set_data ("regionview", this);
		
		fade_out_handle = new ArdourCanvas::SimpleRect (*group);
		fade_out_handle->property_fill_color_rgba() = RGBA_TO_UINT(r,g,b,0);
		fade_out_handle->property_outline_pixels() = 0;
		fade_out_handle->property_y1() = 2.0;
		fade_out_handle->property_y2() = 7.0;
		
		fade_out_handle->set_data ("regionview", this);
	}

	string foo = _region.name();
	foo += ':';
	foo += "gain";

	gain_line = new AudioRegionGainLine (foo, trackview.session(), *this, *group, audio_region().envelope());

	if (!(_flags & EnvelopeVisible)) {
		gain_line->hide ();
	} else {
		gain_line->show ();
	}

	reset_width_dependent_items ((double) _region.length() / samples_per_unit);

	gain_line->reset ();

	set_height (trackview.height);

	region_muted ();
	region_sync_changed ();
	region_resized (BoundsChanged);
	set_waveview_data_src();
	region_locked ();
	envelope_active_changed ();
	fade_in_active_changed ();
	fade_out_active_changed ();

	_region.StateChanged.connect (mem_fun(*this, &AudioRegionView::region_changed));

	fade_in_shape->signal_event().connect (bind (mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_in_event), fade_in_shape, this));
	fade_in_handle->signal_event().connect (bind (mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_in_handle_event), fade_in_handle, this));
	fade_out_shape->signal_event().connect (bind (mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_out_event), fade_out_shape, this));
	fade_out_handle->signal_event().connect (bind (mem_fun (PublicEditor::instance(), &PublicEditor::canvas_fade_out_handle_event), fade_out_handle, this));

	set_colors ();

	/* XXX sync mark drag? */
}

AudioRegionView::~AudioRegionView ()
{
	in_destructor = true;

	RegionViewGoingAway (this); /* EMIT_SIGNAL */

	for (vector<GnomeCanvasWaveViewCache *>::iterator cache = wave_caches.begin(); cache != wave_caches.end() ; ++cache) {
		gnome_canvas_waveview_cache_destroy (*cache);
	}

	/* all waveviews etc will be destroyed when the group is destroyed */

	if (gain_line) {
		delete gain_line;
	}
}

ARDOUR::AudioRegion&
AudioRegionView::audio_region() const
{
	// "Guaranteed" to succeed...
	return dynamic_cast<AudioRegion&>(_region);
}

void
AudioRegionView::region_changed (Change what_changed)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &AudioRegionView::region_changed), what_changed));

	RegionView::region_changed(what_changed);

	if (what_changed & AudioRegion::ScaleAmplitudeChanged) {
		region_scale_amplitude_changed ();
	}
	if (what_changed & AudioRegion::FadeInChanged) {
		fade_in_changed ();
	}
	if (what_changed & AudioRegion::FadeOutChanged) {
		fade_out_changed ();
	}
	if (what_changed & AudioRegion::FadeInActiveChanged) {
		fade_in_active_changed ();
	}
	if (what_changed & AudioRegion::FadeOutActiveChanged) {
		fade_out_active_changed ();
	}
	if (what_changed & AudioRegion::EnvelopeActiveChanged) {
		envelope_active_changed ();
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
AudioRegionView::set_fade_in_active (bool yn)
{
	audio_region().set_fade_in_active (yn);
}

void
AudioRegionView::set_fade_out_active (bool yn)
{
	audio_region().set_fade_out_active (yn);
}

void
AudioRegionView::fade_in_active_changed ()
{
	uint32_t r,g,b,a;
	uint32_t col;
	UINT_TO_RGBA(fade_color,&r,&g,&b,&a);

	if (audio_region().fade_in_active()) {
		col = RGBA_TO_UINT(r,g,b,120);
		fade_in_shape->property_fill_color_rgba() = col;
		fade_in_shape->property_width_pixels() = 0;
		fade_in_shape->property_outline_color_rgba() = RGBA_TO_UINT(r,g,b,0);
	} else { 
		col = RGBA_TO_UINT(r,g,b,0);
		fade_in_shape->property_fill_color_rgba() = col;
		fade_in_shape->property_width_pixels() = 1;
		fade_in_shape->property_outline_color_rgba() = RGBA_TO_UINT(r,g,b,255);
	}
}

void
AudioRegionView::fade_out_active_changed ()
{
	uint32_t r,g,b,a;
	uint32_t col;
	UINT_TO_RGBA(fade_color,&r,&g,&b,&a);

	if (audio_region().fade_out_active()) {
		col = RGBA_TO_UINT(r,g,b,120);
		fade_out_shape->property_fill_color_rgba() = col;
		fade_out_shape->property_width_pixels() = 0;
		fade_out_shape->property_outline_color_rgba() = RGBA_TO_UINT(r,g,b,0);
	} else { 
		col = RGBA_TO_UINT(r,g,b,0);
		fade_out_shape->property_fill_color_rgba() = col;
		fade_out_shape->property_width_pixels() = 1;
		fade_out_shape->property_outline_color_rgba() = RGBA_TO_UINT(r,g,b,255);
	}
}


void
AudioRegionView::region_scale_amplitude_changed ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &AudioRegionView::region_scale_amplitude_changed));

	for (uint32_t n = 0; n < waves.size(); ++n) {
		// force a reload of the cache
		waves[n]->property_data_src() = &_region;
	}
}

void
AudioRegionView::region_resized (Change what_changed)
{
	RegionView::region_resized(what_changed);

	if (what_changed & Change (StartChanged|LengthChanged)) {

	 	for (uint32_t n = 0; n < waves.size(); ++n) {
 			waves[n]->property_region_start() = _region.start();
		}
		
 		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {

 			for (vector<WaveView*>::iterator w = (*i)->waves.begin(); w != (*i)->waves.end(); ++w) {
				(*w)->property_region_start() = _region.start();
 			}
 		}
	}
}

void
AudioRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);
	assert(_pixel_width == pixel_width);

	if (zero_line) {
		zero_line->property_x2() = pixel_width - 1.0;
	}

	if (fade_in_handle) {
		if (pixel_width <= 6.0) {
			fade_in_handle->hide();
			fade_out_handle->hide();
		} else {
			if (_height < 5.0) {
				fade_in_handle->hide();
				fade_out_handle->hide();
			} else {
				fade_in_handle->show();
				fade_out_handle->show();
			}
		}
	}

	reset_fade_shapes ();
}

void
AudioRegionView::region_muted ()
{
	RegionView::region_muted();

	for (uint32_t n=0; n < waves.size(); ++n) {
		if (_region.muted()) {
			waves[n]->property_wave_color() = color_map[cMutedWaveForm];
		} else {
			waves[n]->property_wave_color() = color_map[cWaveForm];
		}
	}
}


void
AudioRegionView::set_height (gdouble height)
{
	uint32_t wcnt = waves.size();

	// FIXME: ick
	TimeAxisViewItem::set_height (height - 2);
	
	_height = height;

	for (uint32_t n=0; n < wcnt; ++n) {
		gdouble ht;

		if ((height) <= NAME_HIGHLIGHT_THRESH) {
			ht = ((height-2*wcnt) / (double) wcnt);
		} else {
			ht = (((height-2*wcnt) - NAME_HIGHLIGHT_SIZE) / (double) wcnt);
		}
		
		gdouble yoff = n * (ht+1);
		
		waves[n]->property_height() = ht;
		waves[n]->property_y() = yoff + 2;
	}

	if (gain_line) {
		if ((height/wcnt) < NAME_HIGHLIGHT_SIZE) {
			gain_line->hide ();
		} else {
			if (_flags & EnvelopeVisible) {
				gain_line->show ();
			}
		}
		gain_line->set_height ((uint32_t) rint (height - NAME_HIGHLIGHT_SIZE));
	}

	manage_zero_line ();
	reset_fade_shapes ();
	
	if (name_text) {
		name_text->raise_to_top();
	}
}

void
AudioRegionView::manage_zero_line ()
{
	if (!zero_line) {
		return;
	}

	if (_height >= 100) {
		gdouble wave_midpoint = (_height - NAME_HIGHLIGHT_SIZE) / 2.0;
		zero_line->property_y1() = wave_midpoint;
		zero_line->property_y2() = wave_midpoint;
		zero_line->show();
	} else {
		zero_line->hide();
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
	reset_fade_in_shape_width ((jack_nframes_t) audio_region().fade_in().back()->when);
}
	
void
AudioRegionView::reset_fade_in_shape_width (jack_nframes_t width)
{
	if (fade_in_handle == 0) {
		return;
	}

	/* smallest size for a fade is 64 frames */

	width = std::max ((jack_nframes_t) 64, width);

	Points* points;
	double pwidth = width / samples_per_unit;
	uint32_t npoints = std::min (gdk_screen_width(), (int) pwidth);
	double h; 
	
	if (_height < 5) {
		fade_in_shape->hide();
		fade_in_handle->hide();
		return;
	}

	double handle_center;
	handle_center = pwidth;
	
	if (handle_center > 7.0) {
		handle_center -= 3.0;
	} else {
		handle_center = 3.0;
	}
	
	fade_in_handle->property_x1() =  handle_center - 3.0;
	fade_in_handle->property_x2() =  handle_center + 3.0;
	
	if (pwidth < 5) {
		fade_in_shape->hide();
		return;
	}

	fade_in_shape->show();

	float curve[npoints];
	audio_region().fade_in().get_vector (0, audio_region().fade_in().back()->when, curve, npoints);

	points = get_canvas_points ("fade in shape", npoints+3);

	if (_height > NAME_HIGHLIGHT_THRESH) {
		h = _height - NAME_HIGHLIGHT_SIZE;
	} else {
		h = _height;
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
}

void
AudioRegionView::reset_fade_out_shape ()
{
	reset_fade_out_shape_width ((jack_nframes_t) audio_region().fade_out().back()->when);
}

void
AudioRegionView::reset_fade_out_shape_width (jack_nframes_t width)
{	
	if (fade_out_handle == 0) {
		return;
	}

	/* smallest size for a fade is 64 frames */

	width = std::max ((jack_nframes_t) 64, width);

	Points* points;
	double pwidth = width / samples_per_unit;
	uint32_t npoints = std::min (gdk_screen_width(), (int) pwidth);
	double h;

	if (_height < 5) {
		fade_out_shape->hide();
		fade_out_handle->hide();
		return;
	}

	double handle_center;
	handle_center = (_region.length() - width) / samples_per_unit;
	
	if (handle_center > 7.0) {
		handle_center -= 3.0;
	} else {
		handle_center = 3.0;
	}
	
	fade_out_handle->property_x1() =  handle_center - 3.0;
	fade_out_handle->property_x2() =  handle_center + 3.0;

	/* don't show shape if its too small */
	
	if (pwidth < 5) {
		fade_out_shape->hide();
		return;
	} 
	
	fade_out_shape->show();

	float curve[npoints];
	audio_region().fade_out().get_vector (0, audio_region().fade_out().back()->when, curve, npoints);

	if (_height > NAME_HIGHLIGHT_THRESH) {
		h = _height - NAME_HIGHLIGHT_SIZE;
	} else {
		h = _height;
	}

	/* points *MUST* be in anti-clockwise order */

	points = get_canvas_points ("fade out shape", npoints+3);

	uint32_t pi, pc;
	double xdelta = pwidth/npoints;

	for (pi = 0, pc = 0; pc < npoints; ++pc) {
		(*points)[pi].set_x(_pixel_width - 1 - pwidth + (pc*xdelta));
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
}

void
AudioRegionView::set_samples_per_unit (gdouble spu)
{
	RegionView::set_samples_per_unit (spu);

	for (uint32_t n=0; n < waves.size(); ++n) {
		waves[n]->property_samples_per_unit() = spu;
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
AudioRegionView::compute_colors (Gdk::Color& basic_color)
{
	RegionView::compute_colors(basic_color);
	
	uint32_t r, g, b, a;

	/* gain color computed in envelope_active_changed() */

	UINT_TO_RGBA (fill_color, &r, &g, &b, &a);
	fade_color = RGBA_TO_UINT(r,g,b,120);
}

void
AudioRegionView::set_colors ()
{
	RegionView::set_colors();
	
	if (gain_line) {
		gain_line->set_line_color (audio_region().envelope_active() ? color_map[cGainLine] : color_map[cGainLineInactive]);
	}

	for (uint32_t n=0; n < waves.size(); ++n) {
		if (_region.muted()) {
			waves[n]->property_wave_color() = color_map[cMutedWaveForm];
		} else {
			waves[n]->property_wave_color() = color_map[cWaveForm];
		}
	}
}

void
AudioRegionView::show_region_editor ()
{
	if (editor == 0) {
		editor = new AudioRegionEditor (trackview.session(), audio_region(), *this);
		// GTK2FIX : how to ensure float without realizing
		// editor->realize ();
		// trackview.editor.ensure_float (*editor);
	} 

	editor->show_all ();
	editor->get_window()->raise();
}

void
AudioRegionView::set_waveform_visible (bool yn)
{
	if (((_flags & WaveformVisible) != yn)) {
		if (yn) {
			for (uint32_t n=0; n < waves.size(); ++n) {
				waves[n]->show();
			}
			_flags |= WaveformVisible;
		} else {
			for (uint32_t n=0; n < waves.size(); ++n) {
				waves[n]->hide();
			}
			_flags &= ~WaveformVisible;
		}
		store_flags ();
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
	if (gain_line && (_flags & EnvelopeVisible)) {
		gain_line->show ();
	}
}

void
AudioRegionView::set_envelope_visible (bool yn)
{
	if (gain_line && ((_flags & EnvelopeVisible) != yn)) {
		if (yn) {
			gain_line->show ();
			_flags |= EnvelopeVisible;
		} else {
			gain_line->hide ();
			_flags &= ~EnvelopeVisible;
		}
		store_flags ();
	}
}

void
AudioRegionView::create_waves ()
{
	bool create_zero_line = true;

	RouteTimeAxisView& atv (*(dynamic_cast<RouteTimeAxisView*>(&trackview))); // ick

	if (!atv.get_diskstream()) {
		return;
	}

	uint32_t nchans = atv.get_diskstream()->n_channels();
	
	/* in tmp_waves, set up null pointers for each channel so the vector is allocated */
	for (uint32_t n = 0; n < nchans; ++n) {
		tmp_waves.push_back (0);
	}
	
	for (uint32_t n = 0; n < nchans; ++n) {
		
		if (n >= audio_region().n_channels()) {
			break;
		}
		
		wave_caches.push_back (WaveView::create_cache ());

		if (wait_for_data) {
			if (audio_region().source(n).peaks_ready (bind (mem_fun(*this, &AudioRegionView::peaks_ready_handler), n), data_ready_connection)) {
				create_one_wave (n, true);
			} else {
				create_zero_line = false;
			}
		} else {
			create_one_wave (n, true);
		}
	}

	if (create_zero_line) {
		zero_line = new ArdourCanvas::SimpleLine (*group);
		zero_line->property_x1() = (gdouble) 1.0;
		zero_line->property_x2() = (gdouble) (_region.length() / samples_per_unit) - 1.0;
		zero_line->property_color_rgba() = (guint) color_map[cZeroLine];
		manage_zero_line ();
	}
}

void
AudioRegionView::create_one_wave (uint32_t which, bool direct)
{
	RouteTimeAxisView& atv (*(dynamic_cast<RouteTimeAxisView*>(&trackview))); // ick
	uint32_t nchans = atv.get_diskstream()->n_channels();
	uint32_t n;
	uint32_t nwaves = std::min (nchans, audio_region().n_channels());
	gdouble ht;

	if (trackview.height < NAME_HIGHLIGHT_SIZE) {
		ht = ((trackview.height) / (double) nchans);
	} else {
		ht = ((trackview.height - NAME_HIGHLIGHT_SIZE) / (double) nchans);
	}

	gdouble yoff = which * ht;

	WaveView *wave = new WaveView(*group);

	wave->property_data_src() = (gpointer) &_region;
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
	wave->property_wave_color() = _region.muted() ? color_map[cMutedWaveForm] : color_map[cWaveForm];
	wave->property_region_start() = _region.start();

	if (!(_flags & WaveformVisible)) {
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

		if (!zero_line) {
			zero_line = new ArdourCanvas::SimpleLine (*group);
			zero_line->property_x1() = (gdouble) 1.0;
			zero_line->property_x2() = (gdouble) (_region.length() / samples_per_unit) - 1.0;
			zero_line->property_color_rgba() = (guint) color_map[cZeroLine];
			manage_zero_line ();
		}
	}
}

void
AudioRegionView::peaks_ready_handler (uint32_t which)
{
	Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun(*this, &AudioRegionView::create_one_wave), which, false));

	if (!waves.empty()) {
		/* all waves created, don't hook into peaks ready anymore */
		data_ready_connection.disconnect ();		
	}
}

void
AudioRegionView::add_gain_point_event (ArdourCanvas::Item *item, GdkEvent *ev)
{
	if (gain_line == 0) {
		return;
	}

	double x, y;

	/* don't create points that can't be seen */

	set_envelope_visible (true);
	
	x = ev->button.x;
	y = ev->button.y;

	item->w2i (x, y);

	jack_nframes_t fx = trackview.editor.pixel_to_frame (x);

	if (fx > _region.length()) {
		return;
	}

	/* compute vertical fractional position */

	y = 1.0 - (y / (trackview.height - NAME_HIGHLIGHT_SIZE));
	
	/* map using gain line */

	gain_line->view_to_model_y (y);

	trackview.session().begin_reversible_command (_("add gain control point"));
	trackview.session().add_undo (audio_region().envelope().get_memento());


	if (!audio_region().envelope_active()) {
		trackview.session().add_undo( bind( mem_fun(audio_region(), &AudioRegion::set_envelope_active), false) );
		audio_region().set_envelope_active(true);
		trackview.session().add_redo( bind( mem_fun(audio_region(), &AudioRegion::set_envelope_active), true) );
	}

	audio_region().envelope().add (fx, y);
	
	trackview.session().add_redo_no_execute (audio_region().envelope().get_memento());
	trackview.session().commit_reversible_command ();
}

void
AudioRegionView::remove_gain_point_event (ArdourCanvas::Item *item, GdkEvent *ev)
{
        ControlPoint *cp = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"));
	audio_region().envelope().erase (cp->model);
}

void
AudioRegionView::store_flags()
{
	XMLNode *node = new XMLNode ("GUI");

	node->add_property ("waveform-visible", (_flags & WaveformVisible) ? "yes" : "no");
	node->add_property ("envelope-visible", (_flags & EnvelopeVisible) ? "yes" : "no");

	_region.add_extra_xml (*node);
}

void
AudioRegionView::set_flags (XMLNode* node)
{
	XMLProperty *prop;

	if ((prop = node->property ("waveform-visible")) != 0) {
		if (prop->value() == "yes") {
			_flags |= WaveformVisible;
		}
	}

	if ((prop = node->property ("envelope-visible")) != 0) {
		if (prop->value() == "yes") {
			_flags |= EnvelopeVisible;
		}
	}
}
	
void
AudioRegionView::set_waveform_shape (WaveformShape shape)
{
	bool yn;

	/* this slightly odd approach is to leave the door open to 
	   other "shapes" such as spectral displays, etc.
	*/

	switch (shape) {
	case Rectified:
		yn = true;
		break;

	default:
		yn = false;
		break;
	}

	if (yn != (bool) (_flags & WaveformRectified)) {
		for (vector<WaveView *>::iterator wave = waves.begin(); wave != waves.end() ; ++wave) {
			(*wave)->property_rectified() = yn;
		}

		if (zero_line) {
			if (yn) {
				zero_line->hide();
			} else {
				zero_line->show();
			}
		}

		if (yn) {
			_flags |= WaveformRectified;
		} else {
			_flags &= ~WaveformRectified;
		}
	}
}

GhostRegion*
AudioRegionView::add_ghost (AutomationTimeAxisView& atv)
{
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&trackview);
	assert(rtv);

	double unit_position = _region.position () / samples_per_unit;
	GhostRegion* ghost = new GhostRegion (atv, unit_position);
	uint32_t nchans;
	
	nchans = rtv->get_diskstream()->n_channels();

	for (uint32_t n = 0; n < nchans; ++n) {
		
		if (n >= audio_region().n_channels()) {
			break;
		}
		
		WaveView *wave = new WaveView(*ghost->group);

		wave->property_data_src() =  &_region;
		wave->property_cache() =  wave_caches[n];
		wave->property_cache_updater() = false;
		wave->property_channel() = n;
		wave->property_length_function() = (gpointer)region_length_from_c;
		wave->property_sourcefile_length_function() = (gpointer) sourcefile_length_from_c;
		wave->property_peak_function() =  (gpointer) region_read_peaks_from_c;
		wave->property_x() =  0.0;
		wave->property_samples_per_unit() =  samples_per_unit;
		wave->property_amplitude_above_axis() =  _amplitude_above_axis;
		wave->property_wave_color() = color_map[cGhostTrackWave];
		wave->property_region_start() = _region.start();

		ghost->waves.push_back(wave);
	}

	ghost->set_height ();
	ghost->set_duration (_region.length() / samples_per_unit);
	ghosts.push_back (ghost);

	ghost->GoingAway.connect (mem_fun(*this, &AudioRegionView::remove_ghost));

	return ghost;
}

void
AudioRegionView::entered ()
{
	if (gain_line && _flags & EnvelopeVisible) {
		gain_line->show_all_control_points ();
	}

	uint32_t r,g,b,a;
	UINT_TO_RGBA(fade_color,&r,&g,&b,&a);
	a=255;
	
	if (fade_in_handle) {
		fade_in_handle->property_fill_color_rgba() = RGBA_TO_UINT(r,g,b,a);
		fade_out_handle->property_fill_color_rgba() = RGBA_TO_UINT(r,g,b,a);
	}
}

void
AudioRegionView::exited ()
{
	if (gain_line) {
		gain_line->hide_all_but_selected_control_points ();
	}
	
	uint32_t r,g,b,a;
	UINT_TO_RGBA(fade_color,&r,&g,&b,&a);
	a=0;
	
	if (fade_in_handle) {
		fade_in_handle->property_fill_color_rgba() = RGBA_TO_UINT(r,g,b,a);
		fade_out_handle->property_fill_color_rgba() = RGBA_TO_UINT(r,g,b,a);
	}
}

void
AudioRegionView::envelope_active_changed ()
{
	if (gain_line) {
		gain_line->set_line_color (audio_region().envelope_active() ? color_map[cGainLine] : color_map[cGainLineInactive]);
	}
}

void
AudioRegionView::set_waveview_data_src()
{

	double unit_length= _region.length() / samples_per_unit;

	for (uint32_t n = 0; n < waves.size(); ++n) {
		// TODO: something else to let it know the channel
		waves[n]->property_data_src() = &_region;
	}
	
	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		
		(*i)->set_duration (unit_length);
		
		for (vector<WaveView*>::iterator w = (*i)->waves.begin(); w != (*i)->waves.end(); ++w) {
			(*w)->property_data_src() = &_region;
		}
	}

}

void
AudioRegionView::color_handler (ColorID id, uint32_t val)
{
	switch (id) {
	case cMutedWaveForm:
	case cWaveForm:
		set_colors ();
		break;

	case cGainLineInactive:
	case cGainLine:
		envelope_active_changed();
		break;
		
	case cZeroLine:
		if (zero_line) {
			zero_line->property_color_rgba() = (guint) color_map[cZeroLine];
		}
		break;

	case cGhostTrackWave:
		break;

	default:
		break;
	}
}
