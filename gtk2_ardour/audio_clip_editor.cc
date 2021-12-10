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
#include "ardour/session.h"

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
{
	set_background_color (UIConfiguration::instance().color (X_("theme:bg")));

	const double scale = UIConfiguration::instance().get_ui_scale();

	frame = new Rectangle (root());
	frame->name = "audio clip editor frame";
	frame->set_fill (false);
	frame->set_outline_color (UIConfiguration::instance().color (X_("theme:darkest")));
	frame->Event.connect (sigc::mem_fun (*this, &AudioClipEditor::event_handler));
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
		WaveView* wv = new WaveView (frame, r);
		wv->set_channel (n);
		waves.push_back (wv);
	}

	std::cerr << "Now have " << waves.size() << " waves" << std::endl;

	set_wave_heights (frame->get().height() - 2.0);
	set_waveform_colors ();
}

void
AudioClipEditor::on_size_allocate (Gtk::Allocation& alloc)
{
	GtkCanvas::on_size_allocate (alloc);

	ArdourCanvas::Rect r (1, 1, alloc.get_width() - 2, alloc.get_height() - 2);
	frame->set (r);

	set_wave_heights (r.height() - 2.0);
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
		wave->set_samples_per_pixel (256);
		wave->set_show_zero_line (false);
		wave->set_clip_level (1.0);
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
