/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/actions.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/utils.h"

#include "waveview/wave_view.h"

#include "ardour/audioregion.h"
#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/source.h"

#include "widgets/ardour_button.h"

#include "audio_clip_editor.h"
#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace ArdourWaveView;
using std::min;
using std::max;

/* ------------ */

AudioClipEditor::AudioClipEditor ()
	: spp (0)
{
	const double scale = UIConfiguration::instance().get_ui_scale();

	frame = new Rectangle (root());
	frame->name = "audio clip editor frame";
	frame->set_fill (false);
	frame->Event.connect (sigc::mem_fun (*this, &AudioClipEditor::event_handler));

	waves_container = new ArdourCanvas::Container (frame);
	line_container = new ArdourCanvas::Container (frame);

	const double line_width = 3.;

	start_line = new Line (line_container);
	start_line->set (Duple (10, 0), Duple (10, 1));
	start_line->set_outline_width (line_width * scale);
	end_line = new Line (line_container);
	end_line->set (Duple (30, 0), Duple (30, 1));
	end_line->set_outline_width (line_width * scale);
	loop_line = new Line (line_container);
	loop_line->set (Duple (50, 0), Duple (50, 1));
	loop_line->set_outline_width (line_width * scale);

	/* hide lines until there is a region */

	line_container->hide ();

	set_colors ();
}

AudioClipEditor::~AudioClipEditor ()
{
	drop_waves ();
}

void
AudioClipEditor::set_colors ()
{
	set_background_color (UIConfiguration::instance().color (X_("theme:bg")));

	frame->set_outline_color (UIConfiguration::instance().color (X_("neutral:midground")));

	start_line->set_outline_color (UIConfiguration::instance().color (X_("theme:contrasting clock")));
	end_line->set_outline_color (UIConfiguration::instance().color (X_("theme:contrasting alt")));
	loop_line->set_outline_color (UIConfiguration::instance().color (X_("theme:contrasting selection")));

	set_waveform_colors ();
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
	samplecnt_t len;

	len = r->source (0)->length().samples ();

	for (uint32_t n = 0; n < n_chans; ++n) {

		boost::shared_ptr<Region> wr = RegionFactory::get_whole_region_for_source (r->source (n));
		if (!wr) {
			continue;
		}

		boost::shared_ptr<AudioRegion> war = boost::dynamic_pointer_cast<AudioRegion> (wr);
		if (!war) {
			continue;
		}

		WaveView* wv = new WaveView (waves_container, war);
		wv->set_channel (n);
		wv->set_show_zero_line (false);
		wv->set_clip_level (1.0);

		waves.push_back (wv);
	}

	set_wave_spp (len);
	set_wave_heights (frame->get().height() - 2.0);
	set_waveform_colors ();

	line_container->show ();
}

void
AudioClipEditor::on_size_allocate (Gtk::Allocation& alloc)
{
	GtkCanvas::on_size_allocate (alloc);

	ArdourCanvas::Rect r (1, 1, alloc.get_width() - 2, alloc.get_height() - 2);
	frame->set (r);

	start_line->set_y1 (frame->get().height() - 2.);
	end_line->set_y1 (frame->get().height() - 2.);
	loop_line->set_y1 (frame->get().height() - 2.);

	set_wave_heights (r.height() - 2.0);
}

void
AudioClipEditor::set_wave_spp (samplecnt_t len)
{
	double available_width = frame->get().width();
	spp = floor (len / available_width);

	for (auto & wave : waves) {
		wave->set_samples_per_pixel (spp);
	}
}

void
AudioClipEditor::set_wave_heights (int h)
{
	if (waves.empty()) {
		return;
	}

	uint32_t n = 0;
	Distance ht = h / waves.size();

	std::cerr << "wave heights: " << ht << std::endl;

	for (auto & wave : waves) {
		wave->set_height (ht);
		wave->set_y_position (n * ht);
		++n;
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
bool
AudioClipEditor::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
//		PublicEditor::instance().get_selection().set (this);
		break;
	case GDK_ENTER_NOTIFY:
//		redraw ();
		break;
	case GDK_LEAVE_NOTIFY:
//		redraw ();
		break;
	default:
		break;
	}

	return false;
}

/* ====================================================== */

AudioClipEditorBox::AudioClipEditorBox ()
{
	_header_label.set_text(_("AUDIO Region Trimmer:"));
	_header_label.set_alignment(0.0, 0.5);
	pack_start(_header_label, false, false, 6);

	editor = manage (new AudioClipEditor);
	editor->set_size_request(600,120);

	pack_start(*editor, true, true);
	editor->show();
}

AudioClipEditorBox::~AudioClipEditorBox ()
{
}

void
AudioClipEditorBox::set_region (boost::shared_ptr<Region> r)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (!ar) {
		return;
	}

	set_session(&r->session());

	state_connection.disconnect();

	_region = r;
	editor->set_region (ar);

	PBD::PropertyChange interesting_stuff;
	region_changed(interesting_stuff);

	_region->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&AudioClipEditorBox::region_changed, this, _1), gui_context());
}

void
AudioClipEditorBox::region_changed (const PBD::PropertyChange& what_changed)
{
//ToDo:  refactor the region_editor.cc  to cover this basic stuff
//	if (what_changed.contains (ARDOUR::Properties::name)) {
//		name_changed ();
//	}

//	PBD::PropertyChange interesting_stuff;
//	interesting_stuff.add (ARDOUR::Properties::length);
//	interesting_stuff.add (ARDOUR::Properties::start);
//	if (what_changed.contains (interesting_stuff))
	{
	}
}
