/*
    Copyright (C) 2001 Paul Davis 

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

    $Id$
*/

#include <cmath>
#include <algorithm>

#include <gtkmm.h>

#include <gtkmm2ext/gtk_ui.h>

#include <ardour/playlist.h>
#include <ardour/audioregion.h>
#include <ardour/sndfilesource.h>
#include <ardour/diskstream.h>

#include "streamview.h"
#include "regionview.h"
#include "audio_time_axis.h"
#include "canvas-simplerect.h"
#include "canvas-simpleline.h"
#include "canvas-waveview.h"
#include "public_editor.h"
#include "region_editor.h"
#include "region_gain_line.h"
#include "ghostregion.h"
#include "audio_time_axis.h"
#include "utils.h"
#include "rgb_macros.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Editing;

static const int32_t sync_mark_width = 9;

sigc::signal<void,AudioRegionView*> AudioRegionView::AudioRegionViewGoingAway;

AudioRegionView::AudioRegionView (GnomeCanvasGroup *parent, AudioTimeAxisView &tv, 
				  AudioRegion& r, 
				  double spu, 
				  double amplitude_above_axis,
				  Gdk::Color& basic_color,
				  bool wfw)

	: TimeAxisViewItem (r.name(), parent, tv, spu, basic_color, r.position(), r.length(),
			    TimeAxisViewItem::Visibility (TimeAxisViewItem::ShowNameText|
				                          TimeAxisViewItem::ShowNameHighlight|
							  TimeAxisViewItem::ShowFrame)),

	  region (r)
{
	GnomeCanvasPoints *shape;
	XMLNode *node;

	editor = 0;
	valid = true;
	in_destructor = false;
	_amplitude_above_axis = amplitude_above_axis;
	zero_line = 0;
	wait_for_waves = wfw;
	_height = 0;

	_flags = 0;

	if ((node = region.extra_xml ("GUI")) != 0) {
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

	gtk_object_set_data (GTK_OBJECT(name_highlight), "regionview", this);
	gtk_object_set_data (GTK_OBJECT(name_text), "regionview", this);

	shape = gnome_canvas_points_new (4);

	/* an equilateral triangle */

	shape->coords[0] = -((sync_mark_width-1)/2);
	shape->coords[1] = 1;

	shape->coords[2] = (sync_mark_width - 1)/2;
	shape->coords[3] = 1;

	shape->coords[4] = 0;
	shape->coords[5] = sync_mark_width - 1;

	shape->coords[6] = -((sync_mark_width-1)/2);
	shape->coords[7] = 1;

	// cerr << "set sync mark al points, nc = " << shape->num_points << endl;
	sync_mark = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
					 gnome_canvas_polygon_get_type(),
					 "points", shape,
					 "fill_color_rgba", fill_color,
					 NULL);
	gnome_canvas_item_hide (sync_mark);
	gnome_canvas_points_unref (shape);

	
	fade_in_shape = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
					     gnome_canvas_polygon_get_type(),
					     "fill_color_rgba", fade_color,
					     NULL);
	gtk_object_set_data (GTK_OBJECT(fade_in_shape), "regionview", this);

	fade_out_shape = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
					      gnome_canvas_polygon_get_type(),
					      "fill_color_rgba", fade_color,
					      NULL);
	gtk_object_set_data (GTK_OBJECT(fade_out_shape), "regionview", this);



	{
			uint32_t r,g,b,a;
			UINT_TO_RGBA(fill_color,&r,&g,&b,&a);
	

	fade_in_handle = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
					      gnome_canvas_simplerect_get_type(),
					      "fill_color_rgba", RGBA_TO_UINT(r,g,b,0),
					      "outline_pixels", 0,
					      "y1", 2.0,
					      "y2", 7.0,
					      NULL);
	gtk_object_set_data (GTK_OBJECT(fade_in_handle), "regionview", this);
	
	fade_out_handle = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
					       gnome_canvas_simplerect_get_type(),
					       "fill_color_rgba", RGBA_TO_UINT(r,g,b,0),
					       "outline_pixels", 0,
					       "y1", 2.0,
					       "y2", 7.0,
					       NULL);
	gtk_object_set_data (GTK_OBJECT(fade_out_handle), "regionview", this);
	}

	string foo = region.name();
	foo += ':';
	foo += "gain";

	gain_line = new AudioRegionGainLine (foo, tv.session(), *this, group, region.envelope(),
					     PublicEditor::canvas_control_point_event,
					     PublicEditor::canvas_line_event);

	if (!(_flags & EnvelopeVisible)) {
		gain_line->hide ();
	} else {
		gain_line->show ();
	}

	reset_width_dependent_items ((double) region.length() / samples_per_unit);

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

	region.StateChanged.connect (mem_fun(*this, &AudioRegionView::region_changed));

	gtk_signal_connect (GTK_OBJECT(group), "event",
			    (GtkSignalFunc) PublicEditor::canvas_region_view_event,
			    this);
	gtk_signal_connect (GTK_OBJECT(name_highlight), "event",
			    (GtkSignalFunc) PublicEditor::canvas_region_view_name_highlight_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(name_text), "event",
			    (GtkSignalFunc) PublicEditor::canvas_region_view_name_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(fade_in_shape), "event",
			    (GtkSignalFunc) PublicEditor::canvas_fade_in_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(fade_in_handle), "event",
			    (GtkSignalFunc) PublicEditor::canvas_fade_in_handle_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(fade_out_shape), "event",
			    (GtkSignalFunc) PublicEditor::canvas_fade_out_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(fade_out_handle), "event",
			    (GtkSignalFunc) PublicEditor::canvas_fade_out_handle_event,
			    this);

	set_colors ();

	/* XXX sync mark drag? */

}

AudioRegionView::~AudioRegionView ()
{
	in_destructor = true;

	AudioRegionViewGoingAway (this); /* EMIT_SIGNAL */

	for (vector<GnomeCanvasWaveViewCache *>::iterator cache = wave_caches.begin(); cache != wave_caches.end() ; ++cache) {
		gnome_canvas_waveview_cache_destroy (*cache);
	}

	/* all waveviews will be destroyed when the group is destroyed */

	for (vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
		delete *g;
	}

	if (editor) {
		delete editor;
	}

	delete gain_line;
}

gint
AudioRegionView::_lock_toggle (GnomeCanvasItem* item, GdkEvent* ev, void* arg)
{
	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		static_cast<AudioRegionView*>(arg)->lock_toggle ();
		return TRUE;
		break;
	default:
		break;
	} 
	return FALSE;
}

void
AudioRegionView::lock_toggle ()
{
	region.set_locked (!region.locked());
}

void
AudioRegionView::region_changed (Change what_changed)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &AudioRegionView::region_changed), what_changed));

	if (what_changed & BoundsChanged) {
		region_resized (what_changed);
		region_sync_changed ();
	}
	if (what_changed & Region::MuteChanged) {
		region_muted ();
	}
	if (what_changed & Region::OpacityChanged) {
		region_opacity ();
	}
	if (what_changed & ARDOUR::NameChanged) {
		region_renamed ();
	}
	if (what_changed & Region::SyncOffsetChanged) {
		region_sync_changed ();
	}
	if (what_changed & Region::LayerChanged) {
		region_layered ();
	}
	if (what_changed & Region::LockChanged) {
		region_locked ();
	}
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
	region.set_fade_in_active (yn);
}

void
AudioRegionView::set_fade_out_active (bool yn)
{
	region.set_fade_out_active (yn);
}

void
AudioRegionView::fade_in_active_changed ()
{
	uint32_t r,g,b,a;
	uint32_t col;
	UINT_TO_RGBA(fade_color,&r,&g,&b,&a);

	if (region.fade_in_active()) {
		col = RGBA_TO_UINT(r,g,b,120);
		gnome_canvas_item_set (fade_in_shape, 
				     "fill_color_rgba", col,
				     "width_pixels", 0,
				     "outline_color_rgba", RGBA_TO_UINT(r,g,b,0),
				     NULL);
	} else { 
		col = RGBA_TO_UINT(r,g,b,0);
		gnome_canvas_item_set (fade_in_shape, 
				     "fill_color_rgba", col,
				     "width_pixels", 1,
				     "outline_color_rgba", RGBA_TO_UINT(r,g,b,255),
				     NULL);
	}
}

void
AudioRegionView::fade_out_active_changed ()
{
	uint32_t r,g,b,a;
	uint32_t col;
	UINT_TO_RGBA(fade_color,&r,&g,&b,&a);

	if (region.fade_out_active()) {
		col = RGBA_TO_UINT(r,g,b,120);
		gnome_canvas_item_set (fade_out_shape, 
				     "fill_color_rgba", col,
				     "width_pixels", 0,
				     "outline_color_rgba", RGBA_TO_UINT(r,g,b,0),
				     NULL);
	} else { 
		col = RGBA_TO_UINT(r,g,b,0);
		gnome_canvas_item_set (fade_out_shape, 
				     "fill_color_rgba", col,
				     "width_pixels", 1,
				     "outline_color_rgba", RGBA_TO_UINT(r,g,b,255),
				     NULL);
	}
}


void
AudioRegionView::region_scale_amplitude_changed ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &AudioRegionView::region_scale_amplitude_changed));

	for (uint32_t n = 0; n < waves.size(); ++n) {
		// force a reload of the cache
		gnome_canvas_item_set (waves[n], "data_src", &region, NULL);
	}
}

void
AudioRegionView::region_locked ()
{
	/* name will show locked status */
	region_renamed ();
}

void
AudioRegionView::region_resized (Change what_changed)
{
	double unit_length;

	if (what_changed & ARDOUR::PositionChanged) {
		set_position (region.position(), 0);
	}

	if (what_changed & Change (StartChanged|LengthChanged)) {

		set_duration (region.length(), 0);

		unit_length = region.length() / samples_per_unit;
		
		reset_width_dependent_items (unit_length);
		
	 	for (uint32_t n = 0; n < waves.size(); ++n) {
 			gnome_canvas_item_set (waves[n], "region_start", (guint32) region.start(), NULL);
 		}
		
 		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {

 			(*i)->set_duration (unit_length);

 			for (vector<GnomeCanvasItem*>::iterator w = (*i)->waves.begin(); w != (*i)->waves.end(); ++w) {
 				gnome_canvas_item_set ((*w), "region_start", region.start(), NULL);
 			}
 		}
	}
}

void
AudioRegionView::reset_width_dependent_items (double pixel_width)
{
	TimeAxisViewItem::reset_width_dependent_items (pixel_width);
	_pixel_width = pixel_width;

	if (zero_line) {
		gnome_canvas_item_set (zero_line, "x2", pixel_width - 1.0, NULL);
	}

	if (pixel_width <= 6.0) {
		gnome_canvas_item_hide (fade_in_handle);
		gnome_canvas_item_hide (fade_out_handle);
	} else {
		if (_height < 5.0) {
			gnome_canvas_item_hide (fade_in_handle);
			gnome_canvas_item_hide (fade_out_handle);
		} else {
			gnome_canvas_item_show (fade_in_handle);
			gnome_canvas_item_show (fade_out_handle);
		}
	}

	reset_fade_shapes ();
}

void
AudioRegionView::region_layered ()
{
	AudioTimeAxisView *atv = dynamic_cast<AudioTimeAxisView*> (&get_time_axis_view());
	atv->view->region_layered (this);
}
	
void
AudioRegionView::region_muted ()
{
	set_frame_color ();
	region_renamed ();

	for (uint32_t n=0; n < waves.size(); ++n) {
		if (region.muted()) {
			gnome_canvas_item_set (waves[n], "wave_color", color_map[cMutedWaveForm], NULL);
		} else {
			gnome_canvas_item_set (waves[n], "wave_color", color_map[cWaveForm], NULL);
		}
	}
}

void
AudioRegionView::region_opacity ()
{
	set_frame_color ();
}

void
AudioRegionView::raise ()
{
	region.raise ();
}

void
AudioRegionView::raise_to_top ()
{
	region.raise_to_top ();
}

void
AudioRegionView::lower ()
{
	region.lower ();
}

void
AudioRegionView::lower_to_bottom ()
{
	region.lower_to_bottom ();
}

bool
AudioRegionView::set_position (jack_nframes_t pos, void* src, double* ignored)
{
	double delta;
	bool ret;

	if (!(ret = TimeAxisViewItem::set_position (pos, this, &delta))) {
		return false;
	}

	if (ignored) {
		*ignored = delta;
	}

	if (delta) {
		for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
			gnome_canvas_item_move ((*i)->group, delta, 0.0);
		}
	}

	return ret;
}

void
AudioRegionView::set_height (gdouble height)
{
	uint32_t wcnt = waves.size();

	TimeAxisViewItem::set_height (height - 2);
	
	_height = height;

	for (uint32_t n=0; n < wcnt; ++n) {
		gdouble ht;

		if ((height) < NAME_HIGHLIGHT_THRESH) {
			ht = ((height-2*wcnt) / (double) wcnt);
		} else {
			ht = (((height-2*wcnt) - NAME_HIGHLIGHT_SIZE) / (double) wcnt);
		}
		
		gdouble yoff = n * (ht+1);
		
		gnome_canvas_item_set (waves[n], "height", ht, NULL);
		gnome_canvas_item_set (waves[n], "y", yoff + 2, NULL);
	}

	if ((height/wcnt) < NAME_HIGHLIGHT_SIZE) {
		gain_line->hide ();
	} else {
		if (_flags & EnvelopeVisible) {
			gain_line->show ();
		}
	}

	manage_zero_line ();
	gain_line->set_height ((uint32_t) rint (height - NAME_HIGHLIGHT_SIZE));
	reset_fade_shapes ();

	gnome_canvas_item_raise_to_top (name_text) ;
}

void
AudioRegionView::manage_zero_line ()
{
	if (!zero_line) {
		return;
	}

	if (_height >= 100) {
		gdouble wave_midpoint = (_height - NAME_HIGHLIGHT_SIZE) / 2.0;
		gnome_canvas_item_set (zero_line, 
				     "y1", wave_midpoint, 
				     "y2", wave_midpoint, 
				     NULL);
		gnome_canvas_item_show (zero_line);
	} else {
		gnome_canvas_item_hide (zero_line);
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
	reset_fade_in_shape_width ((jack_nframes_t) region.fade_in().back()->when);
}
	
void
AudioRegionView::reset_fade_in_shape_width (jack_nframes_t width)
{
	/* smallest size for a fade is 64 frames */

	width = std::max ((jack_nframes_t) 64, width);

	GnomeCanvasPoints* points;
	double pwidth = width / samples_per_unit;
	uint32_t npoints = std::min (gdk_screen_width(), (int) pwidth);
	double h; 
	
	if (_height < 5) {
		gnome_canvas_item_hide (fade_in_shape);
		gnome_canvas_item_hide (fade_in_handle);
		return;
	}

	double handle_center;
	handle_center = pwidth;
	
	if (handle_center > 7.0) {
		handle_center -= 3.0;
	} else {
		handle_center = 3.0;
	}
	
	gnome_canvas_item_set (fade_in_handle, 
			     "x1",  handle_center - 3.0,
			     "x2",  handle_center + 3.0,
			     NULL);
	
	if (pwidth < 5) {
		gnome_canvas_item_hide (fade_in_shape);
		return;
	}

	gnome_canvas_item_show (fade_in_shape);

	float curve[npoints];
	region.fade_in().get_vector (0, region.fade_in().back()->when, curve, npoints);

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
		points->coords[pi++] = 1 + (pc * xdelta);
		points->coords[pi++] = 2 + (h - (curve[pc] * h));
	}
	
	/* fold back */

	points->coords[pi++] = pwidth;
	points->coords[pi++] = 2;

	points->coords[pi++] = 1;
	points->coords[pi++] = 2;

	/* connect the dots ... */

	points->coords[pi++] = points->coords[0];
	points->coords[pi] = points->coords[1];
	
	gnome_canvas_item_set (fade_in_shape, "points", points, NULL);
	gnome_canvas_points_unref (points);
}

void
AudioRegionView::reset_fade_out_shape ()
{
	reset_fade_out_shape_width ((jack_nframes_t) region.fade_out().back()->when);
}

void
AudioRegionView::reset_fade_out_shape_width (jack_nframes_t width)
{	
	/* smallest size for a fade is 64 frames */

	width = std::max ((jack_nframes_t) 64, width);

	GnomeCanvasPoints* points;
	double pwidth = width / samples_per_unit;
	uint32_t npoints = std::min (gdk_screen_width(), (int) pwidth);
	double h;

	if (_height < 5) {
		gnome_canvas_item_hide (fade_out_shape);
		gnome_canvas_item_hide (fade_out_handle);
		return;
	}

	double handle_center;
	handle_center = (region.length() - width) / samples_per_unit;
	
	if (handle_center > 7.0) {
		handle_center -= 3.0;
	} else {
		handle_center = 3.0;
	}
	
	gnome_canvas_item_set (fade_out_handle, 
			     "x1",  handle_center - 3.0,
			     "x2",  handle_center + 3.0,
			     NULL);

	/* don't show shape if its too small */
	
	if (pwidth < 5) {
		gnome_canvas_item_hide (fade_out_shape);
		return;
	} 
	
	gnome_canvas_item_show (fade_out_shape);

	float curve[npoints];
	region.fade_out().get_vector (0, region.fade_out().back()->when, curve, npoints);

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
		points->coords[pi++] = _pixel_width - 1 - pwidth + (pc*xdelta);
		points->coords[pi++] = 2 + (h - (curve[pc] * h));
	}
	
	/* fold back */

	points->coords[pi++] = _pixel_width;
	points->coords[pi++] = h;

	points->coords[pi++] = _pixel_width;
	points->coords[pi++] = 2;

	/* connect the dots ... */

	points->coords[pi++] = points->coords[0];
	points->coords[pi] = points->coords[1];

	gnome_canvas_item_set (fade_out_shape, "points", points, NULL);
	gnome_canvas_points_unref (points);
}

void
AudioRegionView::set_samples_per_unit (gdouble spu)
{
	TimeAxisViewItem::set_samples_per_unit (spu);

	for (uint32_t n=0; n < waves.size(); ++n) {
		gnome_canvas_item_set (waves[n], "samples_per_unit", spu, NULL);
	}

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_samples_per_unit (spu);
		(*i)->set_duration (region.length() / samples_per_unit);
	}

	gain_line->reset ();
	reset_fade_shapes ();
	region_sync_changed ();
}

bool
AudioRegionView::set_duration (jack_nframes_t frames, void *src)
{
	if (!TimeAxisViewItem::set_duration (frames, src)) {
		return false;
	}
	
	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_duration (region.length() / samples_per_unit);
	}

	return true;
}

void
AudioRegionView::set_amplitude_above_axis (gdouble spp)
{
	for (uint32_t n=0; n < waves.size(); ++n) {
		gnome_canvas_item_set (waves[n], "amplitude_above_axis", spp, NULL);
	}
}

void
AudioRegionView::compute_colors (Gdk::Color& basic_color)
{
	TimeAxisViewItem::compute_colors (basic_color);
	uint32_t r, g, b, a;

	/* gain color computed in envelope_active_changed() */

	UINT_TO_RGBA (fill_color, &r, &g, &b, &a);
	fade_color = RGBA_TO_UINT(r,g,b,120);
}

void
AudioRegionView::set_colors ()
{
	TimeAxisViewItem::set_colors ();
	
	gain_line->set_line_color (region.envelope_active() ? color_map[cGainLine] : color_map[cGainLineInactive]);
	gnome_canvas_item_set (sync_mark, "fill_color_rgba", fill_color, NULL);

	for (uint32_t n=0; n < waves.size(); ++n) {
		if (region.muted()) {
			gnome_canvas_item_set (waves[n], "wave_color", color_map[cMutedWaveForm], NULL);
		} else {
			gnome_canvas_item_set (waves[n], "wave_color", color_map[cWaveForm], NULL);
		}
	}
}

void
AudioRegionView::set_frame_color ()
{
	if (region.opaque()) {
		fill_opacity = 180;
	} else {
		fill_opacity = 100;
	}

	TimeAxisViewItem::set_frame_color ();
}

void
AudioRegionView::show_region_editor ()
{
	if (editor == 0) {
		editor = new AudioRegionEditor (trackview.session(), region, *this);
		editor->realize ();
		trackview.editor.ensure_float (*editor);
	} 

	editor->show_all ();
	editor->get_window().raise();
}

void
AudioRegionView::hide_region_editor()
{
	if (editor) {
		editor->hide_all ();
	}
}

void
AudioRegionView::region_renamed ()
{
	string str;

	if (region.locked()) {
		str += '>';
		str += region.name();
		str += '<';
	} else {
		str = region.name();
	}

	if (region.muted()) {
		str = string ("!") + str;
	}

	set_item_name (region.name(), this);
	set_name_text (str);
}

void
AudioRegionView::region_sync_changed ()
{
	int sync_dir;
	jack_nframes_t sync_offset;

	sync_offset = region.sync_offset (sync_dir);

	/* this has to handle both a genuine change of position, a change of samples_per_unit,
	   and a change in the bounds of the region.
	 */

	if (sync_offset == 0) {

		/* no sync mark - its the start of the region */

		gnome_canvas_item_hide (sync_mark);

	} else {

		if ((sync_dir < 0) || ((sync_dir > 0) && (sync_offset > region.length()))) { 

			/* no sync mark - its out of the bounds of the region */

			gnome_canvas_item_hide (sync_mark);

		} else {

			/* lets do it */

			GtkArg args[1];
			GnomeCanvasPoints* points;
			
			args[0].name = X_("points");
			
			gtk_object_getv (GTK_OBJECT(sync_mark), 1, args);
			points = static_cast<GnomeCanvasPoints *> (GTK_VALUE_POINTER(args[0]));
			
			double offset = sync_offset / samples_per_unit;
			
			points->coords[0] = offset - ((sync_mark_width-1)/2);
			points->coords[1] = 1;
			
			points->coords[2] = offset + (sync_mark_width-1)/2;
			points->coords[3] = 1;
			
			points->coords[4] = offset;
			points->coords[5] = sync_mark_width - 1;
			
			points->coords[6] = offset - ((sync_mark_width-1)/2);
			points->coords[7] = 1;
			
			gnome_canvas_item_show (sync_mark);
			gnome_canvas_item_set (sync_mark, "points", points, NULL);

			gnome_canvas_points_unref (points);
		}
	}
}

void
AudioRegionView::set_waveform_visible (bool yn)
{
	if (((_flags & WaveformVisible) != yn)) {
		if (yn) {
			for (uint32_t n=0; n < waves.size(); ++n) {
				gnome_canvas_item_show (waves[n]);
			}
			_flags |= WaveformVisible;
		} else {
			for (uint32_t n=0; n < waves.size(); ++n) {
				gnome_canvas_item_hide (waves[n]);
			}
			_flags &= ~WaveformVisible;
		}
		store_flags ();
	}
}

void
AudioRegionView::temporarily_hide_envelope ()
{
	gain_line->hide ();
}

void
AudioRegionView::unhide_envelope ()
{
	if (_flags & EnvelopeVisible) {
		gain_line->show ();
	}
}

void
AudioRegionView::set_envelope_visible (bool yn)
{
	if ((_flags & EnvelopeVisible) != yn) {
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

	AudioTimeAxisView& atv (*(dynamic_cast<AudioTimeAxisView*>(&trackview))); // ick

	if (!atv.get_diskstream()) {
		return;
	}

	uint32_t nchans = atv.get_diskstream()->n_channels();
	
//	if (wait_for_waves) {
		/* in tmp_waves, set up null pointers for each channel so the vector is allocated */
		for (uint32_t n = 0; n < nchans; ++n) {
			tmp_waves.push_back (0);
		}
//	}
	
	for (uint32_t n = 0; n < nchans; ++n) {
		
		if (n >= region.n_channels()) {
			break;
		}
		
		wave_caches.push_back (gnome_canvas_waveview_cache_new ());

		if (wait_for_waves) {
			if (region.source(n).peaks_ready (bind (mem_fun(*this, &AudioRegionView::peaks_ready_handler), n))) {
				create_one_wave (n, true);
			} else {
				create_zero_line = false;
			}
		} else {
			create_one_wave (n, true);
		}
	}

	if (create_zero_line) {
		zero_line = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
						 gnome_canvas_simpleline_get_type(),
						 "x1", (gdouble) 1.0,
						 "x2", (gdouble) (region.length() / samples_per_unit) - 1.0,
						 "color_rgba", (guint) color_map[cZeroLine],
						 NULL);
		manage_zero_line ();
	}
}

void
AudioRegionView::create_one_wave (uint32_t which, bool direct)
{
	AudioTimeAxisView& atv (*(dynamic_cast<AudioTimeAxisView*>(&trackview))); // ick
	uint32_t nchans = atv.get_diskstream()->n_channels();
	uint32_t n;
	uint32_t nwaves = std::min (nchans, region.n_channels());
	
	gdouble ht;
	if (trackview.height < NAME_HIGHLIGHT_SIZE) {
		ht = ((trackview.height) / (double) nchans);
	} else {
		ht = ((trackview.height - NAME_HIGHLIGHT_SIZE) / (double) nchans);
	}
	gdouble yoff = which * ht;

	GnomeCanvasItem *wave = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
						   gnome_canvas_waveview_get_type (),
						   "data_src", (gpointer) &region,
						   "cache", wave_caches[which],
						   "cache_updater", (gboolean) true,
						   "channel", (guint32) which,
						   "length_function", (gpointer) region_length_from_c,
						   "sourcefile_length_function",(gpointer) sourcefile_length_from_c,
						   "peak_function", (gpointer) region_read_peaks_from_c,
						   "x", 0.0,
						   "y", yoff,
						   "height", (double) ht,
						   "samples_per_unit", samples_per_unit,
						   "amplitude_above_axis", _amplitude_above_axis,
						   "wave_color", (guint32) (region.muted() ? color_map[cMutedWaveForm] : color_map[cWaveForm]),
						   "region_start",(guint32) region.start(),
						   NULL);
	
	if (!(_flags & WaveformVisible)) {
		gnome_canvas_item_hide (wave);
	}

	/* note: calling this function is serialized by the lock
	   held in the peak building thread that signals that
	   peaks are ready for use *or* by the fact that it is
	   called one by one from the GUI thread.
	*/

	if (which < nchans) {
		tmp_waves[which] = (wave);
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
		waves = tmp_waves;
		tmp_waves.clear ();
		
		if (!zero_line) {
			zero_line = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
							 gnome_canvas_simpleline_get_type(),
							 "x1", (gdouble) 1.0,
							 "x2", (gdouble) (region.length() / samples_per_unit) - 1.0,
							 "color_rgba", (guint) color_map[cZeroLine],
							 NULL);
			manage_zero_line ();
		}
	}
}

void
AudioRegionView::peaks_ready_handler (uint32_t which)
{
	Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun(*this, &AudioRegionView::create_one_wave), which, false));
}

void
AudioRegionView::add_gain_point_event (GnomeCanvasItem *item, GdkEvent *ev)
{
	double x, y;

	/* don't create points that can't be seen */

	set_envelope_visible (true);
	
	x = ev->button.x;
	y = ev->button.y;

	gnome_canvas_item_w2i (item, &x, &y);

	jack_nframes_t fx = trackview.editor.pixel_to_frame (x);

	if (fx > region.length()) {
		return;
	}

	/* compute vertical fractional position */

	y = 1.0 - (y / (trackview.height - NAME_HIGHLIGHT_SIZE));
	
	/* map using gain line */

	gain_line->view_to_model_y (y);

	trackview.session().begin_reversible_command (_("add gain control point"));
	trackview.session().add_undo (region.envelope().get_memento());


	if (!region.envelope_active()) {
		trackview.session().add_undo( bind( mem_fun(region, &AudioRegion::set_envelope_active), false) );
		region.set_envelope_active(true);
		trackview.session().add_redo( bind( mem_fun(region, &AudioRegion::set_envelope_active), true) );
	}

	region.envelope().add (fx, y);
	
	trackview.session().add_redo_no_execute (region.envelope().get_memento());
	trackview.session().commit_reversible_command ();
}

void
AudioRegionView::remove_gain_point_event (GnomeCanvasItem *item, GdkEvent *ev)
{
	ControlPoint *cp = reinterpret_cast<ControlPoint *> (gtk_object_get_data(GTK_OBJECT(item), "control_point"));
	region.envelope().erase (cp->model);
}

void
AudioRegionView::store_flags()
{
	XMLNode *node = new XMLNode ("GUI");

	node->add_property ("waveform-visible", (_flags & WaveformVisible) ? "yes" : "no");
	node->add_property ("envelope-visible", (_flags & EnvelopeVisible) ? "yes" : "no");

	region.add_extra_xml (*node);
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
		for (vector<GnomeCanvasItem *>::iterator wave = waves.begin(); wave != waves.end() ; ++wave) {
			gnome_canvas_item_set ((*wave), "rectified", (gboolean) yn, NULL);
		}

		if (zero_line) {
			if (yn) {
				gnome_canvas_item_hide (zero_line);
			} else {
				gnome_canvas_item_show (zero_line);
			}
		}

		if (yn) {
			_flags |= WaveformRectified;
		} else {
			_flags &= ~WaveformRectified;
		}
	}
}

std::string
AudioRegionView::get_item_name ()
{
	return region.name();
}

void
AudioRegionView::move (double x_delta, double y_delta)
{
	if (region.locked() || (x_delta == 0 && y_delta == 0)) {
		return;
	}

	gnome_canvas_item_move (get_canvas_group(), x_delta, y_delta);

	/* note: ghosts never leave their tracks so y_delta for them is always zero */

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		gnome_canvas_item_move ((*i)->group, x_delta, 0.0);
	}
}

GhostRegion*
AudioRegionView::add_ghost (AutomationTimeAxisView& atv)
{
	AudioTimeAxisView& myatv (*(dynamic_cast<AudioTimeAxisView*>(&trackview))); // ick
	double unit_position = region.position () / samples_per_unit;
	GhostRegion* ghost = new GhostRegion (atv, unit_position);
	uint32_t nchans;
	
	nchans = myatv.get_diskstream()->n_channels();

	for (uint32_t n = 0; n < nchans; ++n) {
		
		if (n >= region.n_channels()) {
			break;
		}
		
		GnomeCanvasItem *wave = gnome_canvas_item_new (GNOME_CANVAS_GROUP(ghost->group),
							   gnome_canvas_waveview_get_type (),
							   "data_src", (gpointer) &region,
							   "cache", wave_caches[n],
							   "cache_updater", (gboolean) false,
							   "channel", (guint32) n,
							   "length_function", (gpointer) region_length_from_c,
							   "sourcefile_length_function",(gpointer) sourcefile_length_from_c,
							   "peak_function", (gpointer) region_read_peaks_from_c,
							   "x", 0.0,
							   "samples_per_unit", samples_per_unit,
							   "amplitude_above_axis", _amplitude_above_axis,
							   "wave_color", color_map[cGhostTrackWave],
							   "region_start", (guint32) region.start(),
							   NULL);

		
		ghost->waves.push_back(wave);
	}

	ghost->set_height ();
	ghost->set_duration (region.length() / samples_per_unit);
	ghosts.push_back (ghost);

	ghost->GoingAway.connect (mem_fun(*this, &AudioRegionView::remove_ghost));

	return ghost;
}

void
AudioRegionView::remove_ghost (GhostRegion* ghost)
{
	if (in_destructor) {
		return;
	}

	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		if (*i == ghost) {
			ghosts.erase (i);
			break;
		}
	}
}

uint32_t
AudioRegionView::get_fill_color ()
{
	return fill_color;
}

void
AudioRegionView::entered ()
{
	if (_flags & EnvelopeVisible) {
		gain_line->show_all_control_points ();
	}

	uint32_t r,g,b,a;
	UINT_TO_RGBA(fade_color,&r,&g,&b,&a);
	a=255;
	
	gnome_canvas_item_set (fade_in_handle,  "fill_color_rgba", RGBA_TO_UINT(r,g,b,a), NULL);
	gnome_canvas_item_set (fade_out_handle, "fill_color_rgba", RGBA_TO_UINT(r,g,b,a), NULL);
}

void
AudioRegionView::exited ()
{
	gain_line->hide_all_but_selected_control_points ();
	
	uint32_t r,g,b,a;
	UINT_TO_RGBA(fade_color,&r,&g,&b,&a);
	a=0;
	
	gnome_canvas_item_set (fade_in_handle,  "fill_color_rgba", RGBA_TO_UINT(r,g,b,a), NULL);
	gnome_canvas_item_set (fade_out_handle, "fill_color_rgba", RGBA_TO_UINT(r,g,b,a), NULL);
}

void
AudioRegionView::envelope_active_changed ()
{
	gain_line->set_line_color (region.envelope_active() ? color_map[cGainLine] : color_map[cGainLineInactive]);
}

void
AudioRegionView::set_waveview_data_src()
{

	double unit_length= region.length() / samples_per_unit;

	for (uint32_t n = 0; n < waves.size(); ++n) {
		// TODO: something else to let it know the channel
		gnome_canvas_item_set (waves[n], "data_src", &region, NULL);
	}
	
	for (vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		
		(*i)->set_duration (unit_length);
		
		for (vector<GnomeCanvasItem*>::iterator w = (*i)->waves.begin(); w != (*i)->waves.end(); ++w) {
			gnome_canvas_item_set ((*w), "data_src", &region, NULL);
		}
	}

}


