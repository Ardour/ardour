/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_audio_region_view_h__
#define __gtk_ardour_audio_region_view_h__

#ifdef interface
#undef interface
#endif

#include <vector>

#include <sigc++/signal.h>
#include "ardour/audioregion.h"

#include "canvas/fwd.h"
#include "canvas/xfade_curve.h"

#include "waveview/wave_view.h"

#include "region_view.h"
#include "time_axis_view_item.h"
#include "automation_line.h"
#include "enums.h"

namespace ARDOUR {
	class AudioRegion;
	struct PeakData;
};

class AudioTimeAxisView;
class AudioRegionGainLine;
class GhostRegion;
class AutomationTimeAxisView;
class RouteTimeAxisView;

class AudioRegionView : public RegionView
{
public:
	AudioRegionView (ArdourCanvas::Container *,
	                 RouteTimeAxisView&,
	                 boost::shared_ptr<ARDOUR::AudioRegion>,
	                 double initial_samples_per_pixel,
	                 uint32_t base_color);

	AudioRegionView (ArdourCanvas::Container *,
	                 RouteTimeAxisView&,
	                 boost::shared_ptr<ARDOUR::AudioRegion>,
	                 double samples_per_pixel,
	                 uint32_t base_color,
	                 bool recording,
	                 TimeAxisViewItem::Visibility);

	AudioRegionView (const AudioRegionView& other, boost::shared_ptr<ARDOUR::AudioRegion>);

	~AudioRegionView ();

	void init (bool wait_for_data);

	boost::shared_ptr<ARDOUR::AudioRegion> audio_region() const;

	void create_waves ();
	void delete_waves ();

	void set_height (double);
	void set_samples_per_pixel (double);

	void set_amplitude_above_axis (gdouble spp);

	void temporarily_hide_envelope (); ///< Dangerous!
	void unhide_envelope ();           ///< Dangerous!

	void update_envelope_visibility ();

	void add_gain_point_event (ArdourCanvas::Item *item, GdkEvent *event, bool with_guard_points);
	void remove_gain_point_event (ArdourCanvas::Item *item, GdkEvent *event);

	boost::shared_ptr<AudioRegionGainLine> get_gain_line() const { return gain_line; }

	void region_changed (const PBD::PropertyChange&);
	void envelope_active_changed ();

	GhostRegion* add_ghost (TimeAxisView&);

	void reset_fade_in_shape_width (boost::shared_ptr<ARDOUR::AudioRegion> ar, samplecnt_t, bool drag_active = false);
	void reset_fade_out_shape_width (boost::shared_ptr<ARDOUR::AudioRegion> ar, samplecnt_t, bool drag_active = false);

	samplepos_t get_fade_in_shape_width ();
	samplepos_t get_fade_out_shape_width ();

	void set_fade_visibility (bool);
	void update_coverage_frame (LayerDisplay);

	void update_transient(float old_pos, float new_pos);
	void remove_transient(float pos);

	void show_region_editor ();

	virtual void entered ();
	virtual void exited ();

	void thaw_after_trim ();

	void drag_start ();
	void drag_end ();

	void redraw_start_xfade_to (boost::shared_ptr<ARDOUR::AudioRegion>, samplecnt_t, ArdourCanvas::Points&, double, double);
	void redraw_end_xfade_to (boost::shared_ptr<ARDOUR::AudioRegion>, samplecnt_t, ArdourCanvas::Points&, double, double, double);
	void redraw_start_xfade ();
	void redraw_end_xfade ();

	void hide_xfades ();
	void hide_start_xfade ();
	void hide_end_xfade ();
	void show_xfades ();
	void show_start_xfade ();
	void show_end_xfade ();

	bool start_xfade_visible () const {
		return _start_xfade_visible;
	}

	bool end_xfade_visible () const {
		return _end_xfade_visible;
	}

protected:

	/* this constructor allows derived types
	 * to specify their visibility requirements
	 * to the TimeAxisViewItem parent class
	 */

	enum Flags {
		WaveformVisible = 0x4,
		WaveformRectified = 0x8,
		WaveformLogScaled = 0x10,
	};

	std::vector<ArdourWaveView::WaveView *> waves;
	std::vector<ArdourWaveView::WaveView *> tmp_waves; ///< see \ref create_waves()

	std::list<std::pair<samplepos_t, ArdourCanvas::Line*> > feature_lines;

	ArdourCanvas::Polygon*          sync_mark; ///< polgyon for sync position
	ArdourCanvas::Rectangle*        fade_in_handle; ///< fade in handle, or 0
	ArdourCanvas::Rectangle*        fade_out_handle; ///< fade out handle, or 0
	ArdourCanvas::Rectangle*        fade_in_trim_handle; ///< fade in trim handle, or 0
	ArdourCanvas::Rectangle*        fade_out_trim_handle; ///< fade out trim handle, or 0
	ArdourCanvas::Rectangle*        pending_peak_data;

	static Cairo::RefPtr<Cairo::Pattern> pending_peak_pattern;

	ArdourCanvas::XFadeCurve* start_xfade_curve;
	ArdourCanvas::Rectangle*  start_xfade_rect;
	bool _start_xfade_visible;

	ArdourCanvas::XFadeCurve* end_xfade_curve;
	ArdourCanvas::Rectangle*  end_xfade_rect;
	bool _end_xfade_visible;

	boost::shared_ptr<AudioRegionGainLine> gain_line;

	double _amplitude_above_axis;

	void reset_fade_shapes ();
	void reset_fade_in_shape ();
	void reset_fade_out_shape ();
	void fade_in_changed ();
	void fade_out_changed ();
	void fade_in_active_changed ();
	void fade_out_active_changed ();

	void region_resized (const PBD::PropertyChange&);
	void region_muted ();
	void region_scale_amplitude_changed ();
	void region_renamed ();

	void create_one_wave (uint32_t, bool);
	void peaks_ready_handler (uint32_t);

	void set_colors ();
	void set_waveform_colors ();
	void reset_width_dependent_items (double pixel_width);
	void set_frame_color ();

	void color_handler ();

	void transients_changed();

	AutomationLine::VisibleAspects automation_line_visibility () const;

private:
	void setup_fade_handle_positions ();

	void parameter_changed (std::string const &);
	void setup_waveform_visibility ();
	void set_some_waveform_colors (std::vector<ArdourWaveView::WaveView*>& waves_to_color);

	/** A ScopedConnection for each PeaksReady callback (one per channel).  Each member
	 *  may be 0 if no connection exists.
	 */
	std::vector<PBD::ScopedConnection*> _data_ready_connections;

	/** RegionViews that we hid the xfades for at the start of the current drag;
	 *  first list is for start xfades, second list is for end xfades.
	 */
	std::pair<std::list<AudioRegionView*>, std::list<AudioRegionView*> > _hidden_xfades;

	bool trim_fade_in_drag_active;
	bool trim_fade_out_drag_active;
};

#endif /* __gtk_ardour_audio_region_view_h__ */
