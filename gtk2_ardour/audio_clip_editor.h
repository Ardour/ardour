/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __audio_region_trimmer_box_h__
#define __audio_region_trimmer_box_h__

#include <map>

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"
#include "ardour/triggerbox.h"
#include "ardour/types.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/cairo_packer.h"

#include "widgets/ardour_button.h"

#include "canvas/canvas.h"
#include "canvas/container.h"
#include "canvas/line.h"
#include "canvas/rectangle.h"
#include "canvas/ruler.h"
#include "canvas/scroll_group.h"

#include "audio_clock.h"

namespace ARDOUR
{
	class Session;
	class Location;
	class Trigger;
}

namespace ArdourCanvas
{
	class Text;
	class Polygon;
}

namespace ArdourWaveView
{
	class WaveView;
}

class ClipEditorBox : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	ClipEditorBox () {}
	~ClipEditorBox () {}

	virtual void set_region (boost::shared_ptr<ARDOUR::Region>, ARDOUR::TriggerReference) = 0;

	static void                           init ();
	static void                           register_clip_editor_actions (Gtkmm2ext::Bindings*);
	static Glib::RefPtr<Gtk::ActionGroup> clip_editor_actions;
};

class ClipEditor
{
public:
	virtual ~ClipEditor () {}

	virtual void zoom_in ()  = 0;
	virtual void zoom_out () = 0;
};

class AudioClipEditor : public ArdourCanvas::GtkCanvas
{
public:
	AudioClipEditor ();
	~AudioClipEditor ();

	void set_region (boost::shared_ptr<ARDOUR::AudioRegion>, ARDOUR::TriggerReference);
	void on_size_allocate (Gtk::Allocation&);

	double      sample_to_pixel (ARDOUR::samplepos_t);
	samplepos_t pixel_to_sample (double);

	void   set_spp (double);
	double spp () const
	{
		return _spp;
	}

	bool key_press (GdkEventKey*);

private:
	ArdourCanvas::Rectangle*               frame;
	ArdourCanvas::ScrollGroup*             waves_container;
	ArdourCanvas::Container*               line_container;
	ArdourCanvas::Line*                    start_line;
	ArdourCanvas::Line*                    end_line;
	ArdourCanvas::Line*                    loop_line;
	ArdourCanvas::Rectangle*               scroll_bar_trough;
	ArdourCanvas::Rectangle*               scroll_bar_handle;
	ArdourCanvas::Container*               ruler_container;
	ArdourCanvas::Ruler*                   ruler;

	class ClipBBTMetric : public ArdourCanvas::Ruler::Metric
	{
	  public:
		ClipBBTMetric (ARDOUR::TriggerReference tr) : tref (tr) {
			units_per_pixel = 1;
		}

		void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const;

	  private:
		ARDOUR::TriggerReference tref;

	};

	ClipBBTMetric*                         clip_metric;
	std::vector<ArdourWaveView::WaveView*> waves;
	double                                 non_wave_height;
	samplepos_t                            left_origin;
	double                                 _spp;
	double                                 scroll_fraction;
	boost::shared_ptr<ARDOUR::AudioRegion> audio_region;

	void scroll_left ();
	void scrol_right ();

	enum LineType {
		StartLine,
		EndLine,
		LoopLine,
	};

	bool event_handler (GdkEvent* ev);
	bool line_event_handler (GdkEvent* ev, ArdourCanvas::Line*);
	bool scroll_event_handler (GdkEvent* ev);
	void drop_waves ();
	void set_wave_heights ();
	void set_spp_from_length (ARDOUR::samplecnt_t);
	void set_waveform_colors ();
	void set_colors ();
	void position_lines ();
	void scroll_changed ();

	class LineDrag
	{
	public:
		LineDrag (AudioClipEditor&, ArdourCanvas::Line&);

		void begin (GdkEventButton*);
		void end (GdkEventButton*);
		void motion (GdkEventMotion*);

	private:
		AudioClipEditor&    editor;
		ArdourCanvas::Line& line;
	};

	friend class LineDrag;
	LineDrag* current_line_drag;

	class ScrollDrag
	{
	public:
		ScrollDrag (AudioClipEditor&);

		void begin (GdkEventButton*);
		void end (GdkEventButton*);
		void motion (GdkEventMotion*);

	private:
		AudioClipEditor& editor;
		double           last_x;
	};

	friend class ScrollDrag;
	ScrollDrag* current_scroll_drag;
};

class AudioClipEditorBox : public ClipEditorBox
{
public:
	AudioClipEditorBox ();
	~AudioClipEditorBox ();

	void set_region (boost::shared_ptr<ARDOUR::Region>, ARDOUR::TriggerReference);
	void region_changed (const PBD::PropertyChange& what_changed);

private:
	Gtk::HBox                   header_box;
	ArdourWidgets::ArdourButton zoom_in_button;
	ArdourWidgets::ArdourButton zoom_out_button;
	Gtk::Label                  _header_label;
	Gtk::Table                  table;

	AudioClipEditor* editor;

	PBD::ScopedConnection state_connection;

	boost::shared_ptr<ARDOUR::Region> _region;

	void zoom_in_click ();
	void zoom_out_click ();
};

#endif /* __audio_region_trimmer_box_h__ */
