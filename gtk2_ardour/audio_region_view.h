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

#ifndef __gtk_ardour_audio_region_view_h__
#define __gtk_ardour_audio_region_view_h__

#ifdef interface
#undef interface
#endif

#include <vector>

#include <sigc++/signal.h>
#include "ardour/audioregion.h"

#include "canvas/fwd.h"
#include "canvas/wave_view.h"

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
	AudioRegionView (ArdourCanvas::Group *,
			 RouteTimeAxisView&,
			 boost::shared_ptr<ARDOUR::AudioRegion>,
			 double initial_samples_per_pixel,
			 Gdk::Color const & basic_color);

	AudioRegionView (ArdourCanvas::Group *,
			 RouteTimeAxisView&,
			 boost::shared_ptr<ARDOUR::AudioRegion>,
			 double samples_per_pixel,
			 Gdk::Color const & basic_color,
			 bool recording,
			 TimeAxisViewItem::Visibility);

	AudioRegionView (const AudioRegionView& other, boost::shared_ptr<ARDOUR::AudioRegion>);

	~AudioRegionView ();

	virtual void init (Gdk::Color const & base_color, bool wait_for_data);

	boost::shared_ptr<ARDOUR::AudioRegion> audio_region() const;

	void create_waves ();

	void set_height (double);
	void set_samples_per_pixel (double);

	void set_amplitude_above_axis (gdouble spp);

	void temporarily_hide_envelope (); ///< Dangerous!
	void unhide_envelope ();           ///< Dangerous!

	void update_envelope_visibility ();

	void add_gain_point_event (ArdourCanvas::Item *item, GdkEvent *event);
	void remove_gain_point_event (ArdourCanvas::Item *item, GdkEvent *event);

	boost::shared_ptr<AudioRegionGainLine> get_gain_line() const { return gain_line; }

	void region_changed (const PBD::PropertyChange&);
	void envelope_active_changed ();

	GhostRegion* add_ghost (TimeAxisView&);

	void reset_fade_in_shape_width (boost::shared_ptr<ARDOUR::AudioRegion> ar, framecnt_t);
	void reset_fade_out_shape_width (boost::shared_ptr<ARDOUR::AudioRegion> ar, framecnt_t);

	framepos_t get_fade_in_shape_width ();
	framepos_t get_fade_out_shape_width ();

	void set_fade_visibility (bool);
	void update_coverage_frames (LayerDisplay);

	void update_transient(float old_pos, float new_pos);
	void remove_transient(float pos);

	void show_region_editor ();

	virtual void entered (bool);
	virtual void exited ();

	void thaw_after_trim ();

	void drag_start ();
	void drag_end ();

        void redraw_start_xfade_to (boost::shared_ptr<ARDOUR::AudioRegion>, framecnt_t, ArdourCanvas::Points&, double, double);
        void redraw_end_xfade_to (boost::shared_ptr<ARDOUR::AudioRegion>, framecnt_t, ArdourCanvas::Points&, double, double, double);
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
	   to specify their visibility requirements
	   to the TimeAxisViewItem parent class
	*/

	enum Flags {
		WaveformVisible = 0x4,
		WaveformRectified = 0x8,
		WaveformLogScaled = 0x10,
	};

	std::vector<ArdourCanvas::WaveView *> waves;
	std::vector<ArdourCanvas::WaveView *> tmp_waves; ///< see ::create_waves()

	std::list<std::pair<framepos_t, ArdourCanvas::Line*> > feature_lines;

	ArdourCanvas::Polygon*          sync_mark; ///< polgyon for sync position
	ArdourCanvas::PolyLine*         fade_in_shape;
	ArdourCanvas::PolyLine*         fade_out_shape;
	ArdourCanvas::Rectangle*        fade_in_handle; ///< fade in handle, or 0
	ArdourCanvas::Rectangle*        fade_out_handle; ///< fade out handle, or 0

	ArdourCanvas::PolyLine *start_xfade_in;
	ArdourCanvas::PolyLine *start_xfade_out;
	ArdourCanvas::Rectangle* start_xfade_rect;
	bool _start_xfade_visible;

	ArdourCanvas::PolyLine *end_xfade_in;
	ArdourCanvas::PolyLine *end_xfade_out;
	ArdourCanvas::Rectangle* end_xfade_rect;
	bool _end_xfade_visible;

	boost::shared_ptr<AudioRegionGainLine> gain_line;

	double _amplitude_above_axis;

	uint32_t fade_color;

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
        void set_one_waveform_color (ArdourCanvas::WaveView*);
	void compute_colors (Gdk::Color const &);
	void reset_width_dependent_items (double pixel_width);
	void set_frame_color ();

	void color_handler ();

	void transients_changed();

	AutomationLine::VisibleAspects automation_line_visibility () const;

private:
	void setup_fade_handle_positions ();

	void parameter_changed (std::string const &);
	void setup_waveform_visibility ();
	void setup_waveform_shape ();
	void setup_waveform_scale ();
	void setup_waveform_clipping ();

	/** A ScopedConnection for each PeaksReady callback (one per channel).  Each member
	 *  may be 0 if no connection exists.
	 */
	std::vector<PBD::ScopedConnection*> _data_ready_connections;

	/** RegionViews that we hid the xfades for at the start of the current drag;
	 *  first list is for start xfades, second list is for end xfades.
	 */
	std::pair<std::list<AudioRegionView*>, std::list<AudioRegionView*> > _hidden_xfades;
};

#endif /* __gtk_ardour_audio_region_view_h__ */
