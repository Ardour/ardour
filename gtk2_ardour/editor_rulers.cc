/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <cstdio> // for sprintf, grrr
#include <cmath>
#include <inttypes.h>

#include <string>

#include <gtk/gtkaction.h>

#include "canvas/container.h"
#include "canvas/canvas.h"
#include "canvas/ruler.h"
#include "canvas/debug.h"
#include "canvas/scroll_group.h"

#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/profile.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"

#include "ardour_ui.h"
#include "editor.h"
#include "editing.h"
#include "actions.h"
#include "gui_thread.h"
#include "ruler_dialog.h"
#include "time_axis_view.h"
#include "editor_drag.h"
#include "editor_cursors.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;
using namespace Temporal;

/* the order here must match the "metric" enums in editor.h */

class TimecodeMetric : public ArdourCanvas::Ruler::Metric
{
    public:
	TimecodeMetric (Editor* e) : _editor (e) {}

	void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const {
		_editor->metric_get_timecode (marks, lower, upper, maxchars);
	}

    private:
	Editor* _editor;
};

class SamplesMetric : public ArdourCanvas::Ruler::Metric
{
    public:
	SamplesMetric (Editor* e) : _editor (e) {}

	void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const {
		_editor->metric_get_samples (marks, lower, upper, maxchars);
	}

    private:
	Editor* _editor;
};

class BBTMetric : public ArdourCanvas::Ruler::Metric
{
    public:
	BBTMetric (Editor* e) : _editor (e) {}

	void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const {
		_editor->metric_get_bbt (marks, lower, upper, maxchars);
	}

    private:
	Editor* _editor;
};

class MinsecMetric : public ArdourCanvas::Ruler::Metric
{
    public:
	MinsecMetric (Editor* e) : _editor (e) {}

	void get_marks (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, int maxchars) const {
		_editor->metric_get_minsec (marks, lower, upper, maxchars);
	}

    private:
	Editor* _editor;
};

static ArdourCanvas::Ruler::Metric* _bbt_metric;
static ArdourCanvas::Ruler::Metric* _timecode_metric;
static ArdourCanvas::Ruler::Metric* _samples_metric;
static ArdourCanvas::Ruler::Metric* _minsec_metric;

void
Editor::initialize_rulers ()
{
	ruler_grabbed_widget = 0;

	Pango::FontDescription font (UIConfiguration::instance().get_SmallerFont());
	Pango::FontDescription larger_font (UIConfiguration::instance().get_SmallBoldFont());

	_timecode_metric = new TimecodeMetric (this);
	_bbt_metric = new BBTMetric (this);
	_minsec_metric = new MinsecMetric (this);
	_samples_metric = new SamplesMetric (this);

	timecode_ruler = new ArdourCanvas::Ruler (_time_markers_group, *_timecode_metric,
						  ArdourCanvas::Rect (0, 0, ArdourCanvas::COORD_MAX, timebar_height));
	timecode_ruler->set_font_description (font);
	CANVAS_DEBUG_NAME (timecode_ruler, "timecode ruler");
	timecode_nmarks = 0;

	samples_ruler = new ArdourCanvas::Ruler (_time_markers_group, *_samples_metric,
						 ArdourCanvas::Rect (0, 0, ArdourCanvas::COORD_MAX, timebar_height));
	samples_ruler->set_font_description (font);
	CANVAS_DEBUG_NAME (samples_ruler, "samples ruler");

	minsec_ruler = new ArdourCanvas::Ruler (_time_markers_group, *_minsec_metric,
						ArdourCanvas::Rect (0, 0, ArdourCanvas::COORD_MAX, timebar_height));
	minsec_ruler->set_font_description (font);
	CANVAS_DEBUG_NAME (minsec_ruler, "minsec ruler");
	minsec_nmarks = 0;

	bbt_ruler = new ArdourCanvas::Ruler (_time_markers_group, *_bbt_metric,
	                                     ArdourCanvas::Rect (0, 0, ArdourCanvas::COORD_MAX, timebar_height));
	bbt_ruler->set_font_description (font);
	bbt_ruler->set_second_font_description (larger_font);
	CANVAS_DEBUG_NAME (bbt_ruler, "bbt ruler");
	timecode_nmarks = 0;

	using namespace Box_Helpers;
	BoxList & lab_children =  time_bars_vbox.children();

	lab_children.push_back (Element(minsec_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(timecode_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(samples_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(bbt_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(meter_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(tempo_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(range_mark_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(transport_mark_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(cd_mark_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(mark_label, PACK_SHRINK, PACK_START));
	lab_children.push_back (Element(videotl_label, PACK_SHRINK, PACK_START));

	/* 1 event handler to bind them all ... */

	timecode_ruler->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_ruler_event), timecode_ruler, TimecodeRulerItem));
	minsec_ruler->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_ruler_event), minsec_ruler, MinsecRulerItem));
	bbt_ruler->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_ruler_event), bbt_ruler, BBTRulerItem));
	samples_ruler->Event.connect (sigc::bind (sigc::mem_fun (*this, &Editor::canvas_ruler_event), samples_ruler, SamplesRulerItem));

	visible_timebars = 0; /*this will be changed below */
}

bool
Editor::ruler_label_button_release (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::is_context_menu_event (ev)) {
		if (!ruler_dialog) {
			ruler_dialog = new RulerDialog ();
		}
		ruler_dialog->present ();
	}

	return true;
}

void
Editor::popup_ruler_menu (timepos_t const & where, ItemType t)
{
	using namespace Menu_Helpers;

	if (editor_ruler_menu == 0) {
		editor_ruler_menu = new Menu;
		editor_ruler_menu->set_name ("ArdourContextMenu");
	}

	// always build from scratch
	MenuList& ruler_items = editor_ruler_menu->items();
	editor_ruler_menu->set_name ("ArdourContextMenu");
	ruler_items.clear();

	switch (t) {
	case MarkerBarItem:
		ruler_items.push_back (MenuElem (_("New location marker"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_add_new_marker), where, false)));
		ruler_items.push_back (MenuElem (_("Clear all locations"), sigc::mem_fun(*this, &Editor::clear_markers)));
		ruler_items.push_back (MenuElem (_("Clear all xruns"), sigc::mem_fun(*this, &Editor::clear_xrun_markers)));
		ruler_items.push_back (MenuElem (_("Unhide locations"), sigc::mem_fun(*this, &Editor::unhide_markers)));
		break;

	case RangeMarkerBarItem:
		ruler_items.push_back (MenuElem (_("New range"), sigc::bind (sigc::mem_fun (*this, &Editor::mouse_add_new_range), where)));
		ruler_items.push_back (MenuElem (_("Clear all ranges"), sigc::mem_fun(*this, &Editor::clear_ranges)));
		ruler_items.push_back (MenuElem (_("Unhide ranges"), sigc::mem_fun(*this, &Editor::unhide_ranges)));
		break;

	case TransportMarkerBarItem:
		ruler_items.push_back (MenuElem (_("New Loop range"), sigc::bind (sigc::mem_fun (*this, &Editor::mouse_add_new_loop), where)));
		ruler_items.push_back (MenuElem (_("New Punch range"), sigc::bind (sigc::mem_fun (*this, &Editor::mouse_add_new_punch), where)));
		break;

	case CdMarkerBarItem:
		// TODO
		ruler_items.push_back (MenuElem (_("New CD track marker"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_add_new_marker), where, true)));
		break;

	case TempoBarItem:
		ruler_items.push_back (MenuElem (_("New Tempo"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_add_new_tempo_event), where)));
		break;

	case MeterBarItem:
		ruler_items.push_back (MenuElem (_("New Meter"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_add_new_meter_event), where)));
		break;

	case VideoBarItem:
		/* proper headings would be nice
		 * but AFAICT the only way to get them will be to define a
		 * special GTK style for insensitive Elements or subclass MenuItem
		 */
		//ruler_items.push_back (MenuElem (_("Timeline height"))); // heading
		//static_cast<MenuItem*>(&ruler_items.back())->set_sensitive(false);
		ruler_items.push_back (CheckMenuElem (_("Large"),  sigc::bind (sigc::mem_fun(*this, &Editor::set_video_timeline_height), 6)));
		if (videotl_bar_height == 6) { static_cast<Gtk::CheckMenuItem*>(&ruler_items.back())->set_active(true);}
		ruler_items.push_back (CheckMenuElem (_("Normal"), sigc::bind (sigc::mem_fun(*this, &Editor::set_video_timeline_height), 4)));
		if (videotl_bar_height == 4) { static_cast<Gtk::CheckMenuItem*>(&ruler_items.back())->set_active(true);}
		ruler_items.push_back (CheckMenuElem (_("Small"),  sigc::bind (sigc::mem_fun(*this, &Editor::set_video_timeline_height), 3)));
		if (videotl_bar_height == 3) { static_cast<Gtk::CheckMenuItem*>(&ruler_items.back())->set_active(true);}

		ruler_items.push_back (SeparatorElem ());

		//ruler_items.push_back (MenuElem (_("Align Video Track"))); // heading
		//static_cast<MenuItem*>(&ruler_items.back())->set_sensitive(false);
		ruler_items.push_back (CheckMenuElem (_("Lock")));
		{
			Gtk::CheckMenuItem* vtl_lock = static_cast<Gtk::CheckMenuItem*>(&ruler_items.back());
			vtl_lock->set_active(is_video_timeline_locked());
			vtl_lock->signal_activate().connect (sigc::mem_fun(*this, &Editor::toggle_video_timeline_locked));
		}

		ruler_items.push_back (SeparatorElem ());

		//ruler_items.push_back (MenuElem (_("Video Monitor"))); // heading
		//static_cast<MenuItem*>(&ruler_items.back())->set_sensitive(false);
		ruler_items.push_back (CheckMenuElem (_("Video Monitor")));
		{
			Gtk::CheckMenuItem* xjadeo_toggle = static_cast<Gtk::CheckMenuItem*>(&ruler_items.back());
			if (!ARDOUR_UI::instance()->video_timeline->found_xjadeo()) {
				xjadeo_toggle->set_sensitive(false);
			}
			xjadeo_toggle->set_active(xjadeo_proc_action->get_active());
			xjadeo_toggle->signal_activate().connect (sigc::bind(sigc::mem_fun(*this, &Editor::toggle_xjadeo_proc), -1));
		}
		break;

	default:
		break;
	}

	if (!ruler_items.empty()) {
		editor_ruler_menu->popup (1, gtk_get_current_event_time());
	}

	no_ruler_shown_update = false;
}

void
Editor::store_ruler_visibility ()
{
	XMLNode* node = new XMLNode(X_("RulerVisibility"));

	node->set_property (X_("timecode"), ruler_timecode_action->get_active());
	node->set_property (X_("bbt"), ruler_bbt_action->get_active());
	node->set_property (X_("samples"), ruler_samples_action->get_active());
	node->set_property (X_("minsec"), ruler_minsec_action->get_active());
	node->set_property (X_("tempo"), ruler_tempo_action->get_active());
	node->set_property (X_("meter"), ruler_meter_action->get_active());
	node->set_property (X_("marker"), ruler_marker_action->get_active());
	node->set_property (X_("rangemarker"), ruler_range_action->get_active());
	node->set_property (X_("transportmarker"), ruler_loop_punch_action->get_active());
	node->set_property (X_("cdmarker"), ruler_cd_marker_action->get_active());
	node->set_property (X_("videotl"), ruler_video_action->get_active());

	_session->add_extra_xml (*node);
}

void
Editor::restore_ruler_visibility ()
{
	XMLNode * node = _session->extra_xml (X_("RulerVisibility"));

	no_ruler_shown_update = true;

	bool yn;
	if (node) {
		if (node->get_property ("timecode", yn)) {
			ruler_timecode_action->set_active (yn);
		}
		if (node->get_property ("bbt", yn)) {
			ruler_bbt_action->set_active (yn);
		}
		if (node->get_property ("samples", yn)) {
			ruler_samples_action->set_active (yn);
		}
		if (node->get_property ("minsec", yn)) {
			ruler_minsec_action->set_active (yn);
		}
		if (node->get_property ("tempo", yn)) {
			ruler_tempo_action->set_active (yn);
		}
		if (node->get_property ("meter", yn)) {
			ruler_meter_action->set_active (yn);
		}
		if (node->get_property ("marker", yn)) {
			ruler_marker_action->set_active (yn);
		}
		if (node->get_property ("rangemarker", yn)) {
			ruler_range_action->set_active (yn);
		}
		if (node->get_property ("transportmarker", yn)) {
			ruler_loop_punch_action->set_active (yn);
		}

		if (node->get_property ("cdmarker", yn)) {
				ruler_cd_marker_action->set_active (yn);
		} else {
			// this _session doesn't yet know about the cdmarker ruler
			// as a benefit to the user who doesn't know the feature exists, show the ruler if
			// any cd marks exist
			ruler_cd_marker_action->set_active (false);
			const Locations::LocationList & locs = _session->locations()->list();
			for (Locations::LocationList::const_iterator i = locs.begin(); i != locs.end(); ++i) {
				if ((*i)->is_cd_marker()) {
					ruler_cd_marker_action->set_active (true);
					break;
				}
			}
		}

		if (node->get_property ("videotl", yn)) {
			ruler_video_action->set_active (yn);
		}

	}

	no_ruler_shown_update = false;
	update_ruler_visibility ();
}

void
Editor::update_ruler_visibility ()
{
	int visible_timebars = 0;

	if (no_ruler_shown_update) {
		return;
	}

	/* the order of the timebars is fixed, so we have to go through each one
	 * and adjust its position depending on what is shown.
	 *
	 * Order: minsec, timecode, samples, bbt, meter, tempo, ranges,
	 * loop/punch, cd markers, location markers
	 */

	double tbpos = 0.0;
	double tbgpos = 0.0;
	double old_unit_pos;

#ifdef __APPLE__
	/* gtk update probs require this (damn) */
	meter_label.hide();
	tempo_label.hide();
	range_mark_label.hide();
	transport_mark_label.hide();
	cd_mark_label.hide();
	mark_label.hide();
	videotl_label.hide();
#endif

	if (ruler_minsec_action->get_active()) {
		old_unit_pos = minsec_ruler->position().y;
		if (tbpos != old_unit_pos) {
			minsec_ruler->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		minsec_ruler->show();
		minsec_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		minsec_ruler->hide();
		minsec_label.hide();
	}

	if (ruler_timecode_action->get_active()) {
		old_unit_pos = timecode_ruler->position().y;
		if (tbpos != old_unit_pos) {
			timecode_ruler->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		timecode_ruler->show();
		timecode_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		timecode_ruler->hide();
		timecode_label.hide();
	}

	if (ruler_samples_action->get_active()) {
		old_unit_pos = samples_ruler->position().y;
		if (tbpos != old_unit_pos) {
			samples_ruler->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		samples_ruler->show();
		samples_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		samples_ruler->hide();
		samples_label.hide();
	}

	if (ruler_bbt_action->get_active()) {
		old_unit_pos = bbt_ruler->position().y;
		if (tbpos != old_unit_pos) {
			bbt_ruler->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		bbt_ruler->show();
		bbt_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		bbt_ruler->hide();
		bbt_label.hide();
	}

	if (ruler_meter_action->get_active()) {
		old_unit_pos = meter_group->position().y;
		if (tbpos != old_unit_pos) {
			meter_group->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		meter_group->show();
		meter_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		meter_group->hide();
		meter_label.hide();
	}

	if (ruler_tempo_action->get_active()) {
		old_unit_pos = tempo_group->position().y;
		if (tbpos != old_unit_pos) {
			tempo_group->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		tempo_group->show();
		tempo_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		tempo_group->hide();
		tempo_label.hide();
	}

	if (ruler_range_action->get_active()) {
		old_unit_pos = range_marker_group->position().y;
		if (tbpos != old_unit_pos) {
			range_marker_group->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		range_marker_group->show();
		range_mark_label.show();

		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		range_marker_group->hide();
		range_mark_label.hide();
	}

	if (ruler_loop_punch_action->get_active()) {
		old_unit_pos = transport_marker_group->position().y;
		if (tbpos != old_unit_pos) {
			transport_marker_group->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		transport_marker_group->show();
		transport_mark_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		transport_marker_group->hide();
		transport_mark_label.hide();
	}

	if (ruler_cd_marker_action->get_active()) {
		old_unit_pos = cd_marker_group->position().y;
		if (tbpos != old_unit_pos) {
			cd_marker_group->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		cd_marker_group->show();
		cd_mark_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
		// make sure all cd markers show up in their respective places
		update_cd_marker_display();
	} else {
		cd_marker_group->hide();
		cd_mark_label.hide();
		// make sure all cd markers show up in their respective places
		update_cd_marker_display();
	}

	if (ruler_marker_action->get_active()) {
		old_unit_pos = marker_group->position().y;
		if (tbpos != old_unit_pos) {
			marker_group->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		marker_group->show();
		mark_label.show();
		tbpos += timebar_height;
		tbgpos += timebar_height;
		visible_timebars++;
	} else {
		marker_group->hide();
		mark_label.hide();
	}

	if (ruler_video_action->get_active()) {
		old_unit_pos = videotl_group->position().y;
		if (tbpos != old_unit_pos) {
			videotl_group->move (ArdourCanvas::Duple (0.0, tbpos - old_unit_pos));
		}
		videotl_group->show();
		videotl_label.show();
		tbpos += timebar_height * videotl_bar_height;
		tbgpos += timebar_height * videotl_bar_height;
		visible_timebars+=videotl_bar_height;
		queue_visual_videotimeline_update();
	} else {
		videotl_group->hide();
		videotl_label.hide();
		update_video_timeline(true);
	}

	time_bars_vbox.set_size_request (-1, (int)(timebar_height * visible_timebars));

	/* move hv_scroll_group (trackviews) to the end of the timebars
	 */

	hv_scroll_group->set_y_position (timebar_height * visible_timebars);

	compute_fixed_ruler_scale ();
	update_fixed_rulers();
	redisplay_grid (false);

	/* Changing ruler visibility means that any lines on markers might need updating */
	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		i->second->setup_lines ();
	}
}

void
Editor::update_just_timecode ()
{
	ENSURE_GUI_THREAD (*this, &Editor::update_just_timecode)

	if (_session == 0) {
		return;
	}

	samplepos_t rightmost_sample = _leftmost_sample + current_page_samples();

	if (ruler_timecode_action->get_active()) {
		timecode_ruler->set_range (_leftmost_sample, rightmost_sample);
	}
}

void
Editor::compute_fixed_ruler_scale ()
{
	if (_session == 0) {
		return;
	}

	if (ruler_timecode_action->get_active()) {
		set_timecode_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
	}

	if (ruler_minsec_action->get_active()) {
		set_minsec_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
	}

	if (ruler_samples_action->get_active()) {
		set_samples_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
	}
}

void
Editor::update_fixed_rulers ()
{
	samplepos_t rightmost_sample;

	if (_session == 0) {
		return;
	}

	compute_fixed_ruler_scale ();

	_timecode_metric->units_per_pixel = samples_per_pixel;
	_samples_metric->units_per_pixel = samples_per_pixel;
	_minsec_metric->units_per_pixel = samples_per_pixel;

	rightmost_sample = _leftmost_sample + current_page_samples();

	/* these force a redraw, which in turn will force execution of the metric callbacks
	   to compute the relevant ticks to display.
	*/

	if (ruler_timecode_action->get_active()) {
		timecode_ruler->set_range (_leftmost_sample, rightmost_sample);
	}

	if (ruler_samples_action->get_active()) {
		samples_ruler->set_range (_leftmost_sample, rightmost_sample);
	}

	if (ruler_minsec_action->get_active()) {
		minsec_ruler->set_range (_leftmost_sample, rightmost_sample);
	}
}

void
Editor::update_tempo_based_rulers ()
{
	if (_session == 0) {
		return;
	}

	_bbt_metric->units_per_pixel = samples_per_pixel;

	compute_bbt_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());

	if (ruler_bbt_action->get_active()) {
		bbt_ruler->set_range (_leftmost_sample, _leftmost_sample+current_page_samples());
	}
}


void
Editor::set_timecode_ruler_scale (samplepos_t lower, samplepos_t upper)
{
	using namespace std;

	samplepos_t spacer;
	samplepos_t fr;

	if (_session == 0) {
		return;
	}

	fr = _session->sample_rate();

	if (lower > (spacer = (samplepos_t) (128 * Editor::get_current_zoom ()))) {
		lower = lower - spacer;
	} else {
		lower = 0;
	}

	upper = upper + spacer;
	samplecnt_t const range = upper - lower;

	if (range < (2 * _session->samples_per_timecode_frame())) { /* 0 - 2 samples */
		timecode_ruler_scale = timecode_show_bits;
		timecode_mark_modulo = 20;
		timecode_nmarks = 2 + (2 * _session->config.get_subframes_per_frame());
	} else if (range <= (fr / 4)) { /* 2 samples - 0.250 second */
		timecode_ruler_scale = timecode_show_samples;
		timecode_mark_modulo = 1;
		timecode_nmarks = 2 + (range / (samplepos_t)_session->samples_per_timecode_frame());
	} else if (range <= (fr / 2)) { /* 0.25-0.5 second */
		timecode_ruler_scale = timecode_show_samples;
		timecode_mark_modulo = 2;
		timecode_nmarks = 2 + (range / (samplepos_t)_session->samples_per_timecode_frame());
	} else if (range <= fr) { /* 0.5-1 second */
		timecode_ruler_scale = timecode_show_samples;
		timecode_mark_modulo = 5;
		timecode_nmarks = 2 + (range / (samplepos_t)_session->samples_per_timecode_frame());
	} else if (range <= 2 * fr) { /* 1-2 seconds */
		timecode_ruler_scale = timecode_show_samples;
		timecode_mark_modulo = 10;
		timecode_nmarks = 2 + (range / (samplepos_t)_session->samples_per_timecode_frame());
	} else if (range <= 8 * fr) { /* 2-8 seconds */
		timecode_ruler_scale = timecode_show_seconds;
		timecode_mark_modulo = 1;
		timecode_nmarks = 2 + (range / fr);
	} else if (range <= 16 * fr) { /* 8-16 seconds */
		timecode_ruler_scale = timecode_show_seconds;
		timecode_mark_modulo = 2;
		timecode_nmarks = 2 + (range / fr);
	} else if (range <= 30 * fr) { /* 16-30 seconds */
		timecode_ruler_scale = timecode_show_seconds;
		timecode_mark_modulo = 5;
		timecode_nmarks = 2 + (range / fr);
	} else if (range <= 60 * fr) { /* 30-60 seconds */
		timecode_ruler_scale = timecode_show_seconds;
		timecode_mark_modulo = 5;
		timecode_nmarks = 2 + (range / fr);
	} else if (range <= 2 * 60 * fr) { /* 1-2 minutes */
		timecode_ruler_scale = timecode_show_seconds;
		timecode_mark_modulo = 15;
		timecode_nmarks = 2 + (range / fr);
	} else if (range <= 4 * 60 * fr) { /* 2-4 minutes */
		timecode_ruler_scale = timecode_show_seconds;
		timecode_mark_modulo = 30;
		timecode_nmarks = 2 + (range / fr);
	} else if (range <= 10 * 60 * fr) { /* 4-10 minutes */
		timecode_ruler_scale = timecode_show_minutes;
		timecode_mark_modulo = 2;
		timecode_nmarks = 2 + 10;
	} else if (range <= 30 * 60 * fr) { /* 10-30 minutes */
		timecode_ruler_scale = timecode_show_minutes;
		timecode_mark_modulo = 5;
		timecode_nmarks = 2 + 30;
	} else if (range <= 60 * 60 * fr) { /* 30 minutes - 1hr */
		timecode_ruler_scale = timecode_show_minutes;
		timecode_mark_modulo = 10;
		timecode_nmarks = 2 + 60;
	} else if (range <= 4 * 60 * 60 * fr) { /* 1 - 4 hrs*/
		timecode_ruler_scale = timecode_show_minutes;
		timecode_mark_modulo = 30;
		timecode_nmarks = 2 + (60 * 4);
	} else if (range <= 8 * 60 * 60 * fr) { /* 4 - 8 hrs*/
		timecode_ruler_scale = timecode_show_hours;
		timecode_mark_modulo = 1;
		timecode_nmarks = 2 + 8;
	} else if (range <= 16 * 60 * 60 * fr) { /* 16-24 hrs*/
		timecode_ruler_scale = timecode_show_hours;
		timecode_mark_modulo = 1;
		timecode_nmarks = 2 + 24;
	} else {

		const samplecnt_t hours_in_range = range / (60 * 60 * fr);
		const int text_width_rough_guess = 120; /* pixels, very very approximate guess at how wide the tick mark text is */

		/* Normally we do not need to know anything about the width of the canvas
		   to set the ruler scale, because the caller has already determined
		   the width and set lower + upper arguments to this function to match that.

		   But in this case, where the range defined by lower and uppper can vary
		   substantially (basically anything from 24hrs+ to several billion years)
		   trying to decide which tick marks to show does require us to know
		   about the available width.
		*/

		timecode_nmarks = _track_canvas->width() / text_width_rough_guess;
		timecode_ruler_scale = timecode_show_many_hours;
		timecode_mark_modulo = std::max ((samplecnt_t) 1, 1 + (hours_in_range / timecode_nmarks));
	}
}

void
Editor::metric_get_timecode (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t /*upper*/, gint /*maxchars*/)
{
	samplepos_t pos;
	samplecnt_t spacer;
	Timecode::Time timecode;
	gchar buf[16];
	gint n;
	ArdourCanvas::Ruler::Mark mark;

	if (_session == 0) {
		return;
	}

	if (lower > (spacer = (samplecnt_t)(128 * Editor::get_current_zoom ()))) {
		lower = lower - spacer;
	} else {
		lower = 0;
	}

	pos = (samplecnt_t) floor (lower);

	switch (timecode_ruler_scale) {
	case timecode_show_bits:
		// Find timecode time of this sample (pos) with subframe accuracy
		_session->sample_to_timecode(pos, timecode, true /* use_offset */, true /* use_subframes */);
		for (n = 0; n < timecode_nmarks; n++) {
			_session->timecode_to_sample(timecode, pos, true /* use_offset */, true /* use_subframes */);
			if ((timecode.subframes % timecode_mark_modulo) == 0) {
				if (timecode.subframes == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
					snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", timecode.negative ? "-" : "", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
					snprintf (buf, sizeof(buf), ".%02u", timecode.subframes);
				}
			} else {
				snprintf (buf, sizeof(buf)," ");
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
			}
			mark.label = buf;
			mark.position = pos;
			marks.push_back (mark);
			// Increment subframes by one
			Timecode::increment_subframes (timecode, _session->config.get_subframes_per_frame());
		}
		break;

	case timecode_show_samples:
		// Find timecode time of this sample (pos)
		_session->sample_to_timecode (pos, timecode, true /* use_offset */, false /* use_subframes */);
		// Go to next whole sample down
		Timecode::frames_floot (timecode);
		for (n = 0; n < timecode_nmarks; n++) {
			_session->timecode_to_sample (timecode, pos, true /* use_offset */, false /* use_subframes */);
			if ((timecode.frames % timecode_mark_modulo) == 0) {
				if (timecode.frames == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
				}
				mark.position = pos;
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", timecode.negative ? "-" : "", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
				mark.position = pos;
			}
			mark.label = buf;
			marks.push_back (mark);
			Timecode::increment (timecode, _session->config.get_subframes_per_frame());
		}
		break;

	case timecode_show_seconds:
		// Find timecode time of this sample (pos)
		_session->sample_to_timecode (pos, timecode, true /* use_offset */, false /* use_subframes */);
		// Go to next whole second down
		Timecode::seconds_floor (timecode);
		for (n = 0; n < timecode_nmarks; n++) {
			_session->timecode_to_sample (timecode, pos, true /* use_offset */, false /* use_subframes */);
			if ((timecode.seconds % timecode_mark_modulo) == 0) {
				if (timecode.seconds == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
					mark.position = pos;
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
					mark.position = pos;
				}
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", timecode.negative ? "-" : "", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
				mark.position = pos;
			}
			mark.label = buf;
			marks.push_back (mark);
			Timecode::increment_seconds (timecode, _session->config.get_subframes_per_frame());
		}
		break;

	case timecode_show_minutes:
		//Find timecode time of this sample (pos)
		_session->sample_to_timecode (pos, timecode, true /* use_offset */, false /* use_subframes */);
		// Go to next whole minute down
		Timecode::minutes_floor (timecode);
		for (n = 0; n < timecode_nmarks; n++) {
			_session->timecode_to_sample (timecode, pos, true /* use_offset */, false /* use_subframes */);
			if ((timecode.minutes % timecode_mark_modulo) == 0) {
				if (timecode.minutes == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
				}
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", timecode.negative ? "-" : "", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
			}
			mark.label = buf;
			mark.position = pos;
			marks.push_back (mark);
			Timecode::increment_minutes (timecode, _session->config.get_subframes_per_frame());
		}
		break;
	case timecode_show_hours:
		// Find timecode time of this sample (pos)
		_session->sample_to_timecode (pos, timecode, true /* use_offset */, false /* use_subframes */);
		// Go to next whole hour down
		Timecode::hours_floor (timecode);
		for (n = 0; n < timecode_nmarks; n++) {
			_session->timecode_to_sample (timecode, pos, true /* use_offset */, false /* use_subframes */);
			if ((timecode.hours % timecode_mark_modulo) == 0) {
				mark.style = ArdourCanvas::Ruler::Mark::Major;
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", timecode.negative ? "-" : "", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
			} else {
				snprintf (buf, sizeof(buf)," ");
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
			}
			mark.label = buf;
			mark.position = pos;
			marks.push_back (mark);
			Timecode::increment_hours (timecode, _session->config.get_subframes_per_frame());
		}
		break;
	case timecode_show_many_hours:
		// Find timecode time of this sample (pos)
		_session->sample_to_timecode (pos, timecode, true /* use_offset */, false /* use_subframes */);
		// Go to next whole hour down
		Timecode::hours_floor (timecode);

		for (n = 0; n < timecode_nmarks;) {
			_session->timecode_to_sample (timecode, pos, true /* use_offset */, false /* use_subframes */);
			if ((timecode.hours % timecode_mark_modulo) == 0) {
				mark.style = ArdourCanvas::Ruler::Mark::Major;
				snprintf (buf, sizeof(buf), "%s%02u:%02u:%02u:%02u", timecode.negative ? "-" : "", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
				mark.label = buf;
				mark.position = pos;
				marks.push_back (mark);
				++n;
			}
			/* can't use Timecode::increment_hours() here because we may be traversing thousands of hours
			 * and doing it 1 hour at a time is just stupid (and slow).
			 */
			timecode.hours += timecode_mark_modulo - (timecode.hours % timecode_mark_modulo);
		}
		break;
	}
}

void
Editor::compute_bbt_ruler_scale (samplepos_t lower, samplepos_t upper)
{
	if (_session == 0) {
		return;
	}

	std::vector<Temporal::Point>::const_iterator i;
	Temporal::BBT_Time lower_beat, upper_beat; // the beats at each end of the ruler
	Beats floor_lower_beat = std::max (Beats(), _session->tempo_map().quarter_note_at (lower)).round_down_to_beat ();

	if (floor_lower_beat < 0.0) {
		floor_lower_beat = 0.0;
	}

	const samplepos_t beat_before_lower_pos = _session->tempo_map().sample_at (floor_lower_beat, _session->sample_rate());
	const samplepos_t beat_after_upper_pos = _session->tempo_map().sample_at ((std::max (Beats(), _session->tempo_map().quarter_note_at  (upper)).round_down_to_beat()) + Beats (1, 0), _session->sample_rate());

	_session->bbt_time (timepos_t (beat_before_lower_pos), lower_beat);
	_session->bbt_time (timepos_t (beat_after_upper_pos), upper_beat);
	uint32_t beats = 0;

	bbt_bar_helper_on = false;
	bbt_bars = 0;
	bbt_nmarks = 1;

	bbt_ruler_scale =  bbt_show_many;

	const double ceil_upper_beat = floor (std::max (0.0, _session->tempo_map().beat_at_sample (upper))) + 1.0;

	if (ceil_upper_beat == floor_lower_beat) {
		return;
	}

	bbt_bars = _session->tempo_map().bbt_at (ceil_upper_beat).bars - _session->tempo_map().bbt_at (floor_lower_beat).bars;

	double ruler_line_granularity = UIConfiguration::instance().get_ruler_granularity ();  //in pixels
	ruler_line_granularity = _visible_canvas_width / (ruler_line_granularity*5);  //fudge factor '5' probably related to (4+1 beats)/measure, I think

	beats = (ceil_upper_beat - floor_lower_beat);
	double beat_density = ((beats + 1) * ((double) (upper - lower) / (double) (1 + beat_after_upper_pos - beat_before_lower_pos))) / (float)ruler_line_granularity;

	/* Only show the bar helper if there aren't many bars on the screen */
	if ((bbt_bars < 2) || (beats < 5)) {
		bbt_bar_helper_on = true;
	}

	if (beat_density > 2048) {
		bbt_ruler_scale = bbt_show_many;
	} else if (beat_density > 1024) {
		bbt_ruler_scale = bbt_show_64;
	} else if (beat_density > 256) {
		bbt_ruler_scale = bbt_show_16;
	} else if (beat_density > 64) {
		bbt_ruler_scale = bbt_show_4;
	} else if (beat_density > 16) {
		bbt_ruler_scale = bbt_show_1;
	} else if (beat_density > 4) {
		bbt_ruler_scale =  bbt_show_quarters;
	} else  if (beat_density > 2) {
		bbt_ruler_scale =  bbt_show_eighths;
	} else  if (beat_density > 1) {
		bbt_ruler_scale =  bbt_show_sixteenths;
	} else  if (beat_density > 0.5) {
		bbt_ruler_scale =  bbt_show_thirtyseconds;
	} else  if (beat_density > 0.25) {
		bbt_ruler_scale =  bbt_show_sixtyfourths;
	} else {
		bbt_ruler_scale =  bbt_show_onetwentyeighths;
	}

	/* Now that we know how fine a grid (Ruler) is allowable on this screen, limit it to the coarseness selected by the user */
	/* note: GridType and RulerScale are not the same enums, so it's not a simple mathematical operation */
	int suggested_scale = (int) bbt_ruler_scale;
	int divs = get_grid_music_divisions(_grid_type);
	if (_grid_type == GridTypeBar) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_1);
	} else if (_grid_type == GridTypeBeat) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_quarters);
	}  else if ( divs < 4 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_eighths);
	}  else if ( divs < 8 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_sixteenths);
	} else if ( divs < 16 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_thirtyseconds);
	} else if ( divs < 32 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_sixtyfourths);
	} else {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_onetwentyeighths);
	}

	bbt_ruler_scale = (Editor::BBTRulerScale) suggested_scale;
}

static void
edit_last_mark_label (std::vector<ArdourCanvas::Ruler::Mark>& marks, const std::string& newlabel)
{
	ArdourCanvas::Ruler::Mark copy = marks.back();
	copy.label = newlabel;
	marks.pop_back ();
	marks.push_back (copy);
}

void
Editor::metric_get_bbt (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, gint /*maxchars*/)
{
	if (_session == 0) {
		return;
	}

	Temporal::TempoMapPoints::const_iterator i;

	char buf[64];
	gint  n = 0;
	samplepos_t pos;
	Temporal::BBT_Time next_beat;
	uint32_t beats = 0;
	uint32_t tick = 0;
	uint32_t skip;
	uint32_t t;
	double bbt_position_of_helper;
	bool i_am_accented = false;
	bool helper_active = false;
	ArdourCanvas::Ruler::Mark mark;
	const samplecnt_t sr (_session->sample_rate());

	Temporal::TempoMapPoints grid;

	compute_current_bbt_points (grid, lower, upper);

	if (distance (grid.begin(), grid.end()) == 0) {
		return;
	}

	/* we can accent certain lines depending on the user's Grid choice */
	/* for example, even in a 4/4 meter we can draw a grid with triplet-feel */
	/* and in this case you will want the accents on '3s' not '2s' */
	uint32_t bbt_divisor = 2;
	uint32_t bbt_accent_modulo = 2;
	switch (_grid_type) {
	case GridTypeBeatDiv3:
		bbt_divisor = 3;
		bbt_accent_modulo = 3;
		break;
	case GridTypeBeatDiv5:
		bbt_divisor = 5;
		bbt_accent_modulo = 5;
		break;
	case GridTypeBeatDiv6:
		bbt_divisor = 3;
		bbt_accent_modulo = 3;
		break;
	case GridTypeBeatDiv7:
		bbt_divisor = 7;
		bbt_accent_modulo = 7;
		break;
	case GridTypeBeatDiv10:
		bbt_divisor = 5;
		bbt_accent_modulo = 5;
		break;
	case GridTypeBeatDiv12:
		bbt_divisor = 3;
		bbt_accent_modulo = 3;
		break;
	case GridTypeBeatDiv14:
		bbt_divisor = 7;
		bbt_accent_modulo = 7;
		break;
	case GridTypeBeatDiv16:
		bbt_accent_modulo = 4;
		break;
	case GridTypeBeatDiv20:
		bbt_divisor = 5;
		bbt_accent_modulo = 5;
		break;
	case GridTypeBeatDiv24:
		bbt_divisor = 6;
		bbt_accent_modulo = 6;
		break;
	case GridTypeBeatDiv28:
		bbt_divisor = 7;
		bbt_accent_modulo = 7;
		break;
	case GridTypeBeatDiv32:
		bbt_accent_modulo = 8;
		break;
	default:
		bbt_divisor = 2;
		bbt_accent_modulo = 2;
		break;
	}

	uint32_t bbt_beat_subdivision = 1;
	switch (bbt_ruler_scale) {
	case bbt_show_quarters:
		bbt_beat_subdivision = 1;
		break;
	case bbt_show_eighths:
		bbt_beat_subdivision = 1;
		break;
	case bbt_show_sixteenths:
		bbt_beat_subdivision = 2;
		break;
	case bbt_show_thirtyseconds:
		bbt_beat_subdivision = 4;
		break;
	case bbt_show_sixtyfourths:
		bbt_beat_subdivision = 8;
		break;
	case bbt_show_onetwentyeighths:
		bbt_beat_subdivision = 16;
		break;
	default:
		bbt_beat_subdivision = 1;
		break;
	}

	bbt_beat_subdivision *= bbt_divisor;

	switch (bbt_ruler_scale) {

	case bbt_show_many:
		bbt_nmarks = 1;
		snprintf (buf, sizeof(buf), "cannot handle %" PRIu32 " bars", bbt_bars);
		mark.style = ArdourCanvas::Ruler::Mark::Major;
		mark.label = buf;
		mark.position = lower;
		marks.push_back (mark);
		break;

	case bbt_show_64:
			bbt_nmarks = (gint) (bbt_bars / 64) + 1;
			for (n = 0, i = grid.begin(); i != grid.end() && n < bbt_nmarks; i++) {
				if ((*i).is_bar()) {
					if ((*i).bar % 64 == 1) {
						if ((*i).bar % 256 == 1) {
							snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
							mark.style = ArdourCanvas::Ruler::Mark::Major;
						} else {
							buf[0] = '\0';
							if ((*i).bar % 256 == 129)  {
								mark.style = ArdourCanvas::Ruler::Mark::Minor;
							} else {
								mark.style = ArdourCanvas::Ruler::Mark::Micro;
							}
						}
						mark.label = buf;
						mark.position = (*i).sample;
						marks.push_back (mark);
						++n;
					}
				}
			}
			break;

	case bbt_show_16:
		bbt_nmarks = (bbt_bars / 16) + 1;
		for (n = 0,  i = grid.begin(); i != grid.end() && n < bbt_nmarks; i++) {
			if ((*i).is_bar()) {
			  if ((*i).bar % 16 == 1) {
				if ((*i).bar % 64 == 1) {
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					buf[0] = '\0';
					if ((*i).bar % 64 == 33)  {
						mark.style = ArdourCanvas::Ruler::Mark::Minor;
					} else {
						mark.style = ArdourCanvas::Ruler::Mark::Micro;
					}
				}
				mark.label = buf;
				mark.position = (*i).sample(sr);
				marks.push_back (mark);
				++n;
			  }
			}
		}
	  break;

	case bbt_show_4:
		bbt_nmarks = (bbt_bars / 4) + 1;
		for (n = 0, i = grid.begin(); i != grid.end() && n < bbt_nmarks; ++i) {
			if ((*i).is_bar()) {
				if ((*i).bar % 4 == 1) {
					if ((*i).bar % 16 == 1) {
						snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
						mark.style = ArdourCanvas::Ruler::Mark::Major;
					} else {
						buf[0] = '\0';
						if ((*i).bar % 16 == 9)  {
							mark.style = ArdourCanvas::Ruler::Mark::Minor;
						} else {
							mark.style = ArdourCanvas::Ruler::Mark::Micro;
						}
					}
					mark.label = buf;
					mark.position = (*i).sample;
					marks.push_back (mark);
					++n;
				}
			}
		}
	  break;

	case bbt_show_1:
		bbt_nmarks = bbt_bars + 2;
		for (n = 0,  i = grid.begin(); i != grid.end() && n < bbt_nmarks; ++i) {
			if ((*i).is_bar()) {
				snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bbt().bars);
				mark.style = ArdourCanvas::Ruler::Mark::Major;
				mark.label = buf;
				mark.position = (*i).sample;
				marks.push_back (mark);
				++n;
			}
		}
	break;

	case bbt_show_quarters:

		beats = distance (grid.begin(), grid.end());
		bbt_nmarks = beats + 2;

		mark.label = "";
		mark.position = lower;
		mark.style = ArdourCanvas::Ruler::Mark::Micro;
		marks.push_back (mark);

		for (n = 1, i = grid.begin(); n < bbt_nmarks && i != grid.end(); ++i) {

			if ((*i).sample (sr) < lower && (bbt_bar_helper_on)) {
				snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, (*i).bbt().bars, (*i).bbt().beats);
				edit_last_mark_label (marks, buf);
			} else {

				if ((*i).bbt().is_bar()) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
				} else if (((*i).beat % 2 == 1)) {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
					buf[0] = '\0';
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Micro;
					buf[0] = '\0';
				}
				mark.label = buf;
				mark.position = (*i).sample;
				marks.push_back (mark);
				n++;
			}
		}
		break;

	case bbt_show_eighths:
	case bbt_show_sixteenths:
	case bbt_show_thirtyseconds:
	case bbt_show_sixtyfourths:
	case bbt_show_onetwentyeighths:

		beats = distance (grid.begin(), grid.end());
		bbt_nmarks = (beats + 2) * bbt_beat_subdivision;

		bbt_position_of_helper = lower + (3 * Editor::get_current_zoom ());

		mark.label = "";
		mark.position = lower;
		mark.style = ArdourCanvas::Ruler::Mark::Micro;
		marks.push_back (mark);

		for (n = 1, i = grid.begin(); n < bbt_nmarks && i != grid.end(); ++i) {

			if ((*i).sample (sr) < lower && (bbt_bar_helper_on)) {
				snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, (*i).bbt().bars, (*i).bbt().beats);
				edit_last_mark_label (marks, buf);
				helper_active = true;
			} else {

				if ((*i).bbt().is_bar()) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bbt().bars);
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bbt().beats);
				}
				if (((*i).sample(sr) < bbt_position_of_helper) && helper_active) {
					buf[0] = '\0';
				}
				mark.label =  buf;
				mark.position = (*i).sample (sr);
				marks.push_back (mark);
				n++;
			}

			/* Add the tick marks */
			skip = Temporal::ticks_per_beat / bbt_beat_subdivision;
			tick = skip; // the first non-beat tick

			t = 0;
			while (tick < Temporal::ticks_per_beat && (n < bbt_nmarks)) {

				next_beat.beats = (*i).bbt().beats;
				next_beat.bars = (*i).bbt().bars;
				next_beat.ticks = tick;
				pos = _session->tempo_map().sample_at (next_beat, sr);

				if (t % bbt_accent_modulo == (bbt_accent_modulo - 1)) {
					i_am_accented = true;
				}
				if (i_am_accented && (pos > bbt_position_of_helper)){
					snprintf (buf, sizeof(buf), "%" PRIu32, tick);
				} else {
					buf[0] = '\0';
				}

				mark.label = buf;
				mark.position = pos;

				if ((bbt_beat_subdivision > 4) && i_am_accented) {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Micro;
				}
				i_am_accented = false;
				marks.push_back (mark);

				tick += skip;
				++t;
				++n;
			}
		}

	  break;

	case bbt_show_thirtyseconds:

		beats = distance (grid.begin(), grid.end());
		bbt_nmarks = (beats + 2) * bbt_beat_subdivision;

		bbt_position_of_helper = lower + (3 * Editor::get_current_zoom ());

		mark.label = "";
		mark.position = lower;
		mark.style = ArdourCanvas::Ruler::Mark::Micro;
		marks.push_back (mark);

		for (n = 1, i = grid.begin(); n < bbt_nmarks && i != grid.end(); ++i) {

			if ((*i).sample < lower && (bbt_bar_helper_on)) {
				  snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, (*i).bar, (*i).beat);
				  edit_last_mark_label (marks, buf);
				  helper_active = true;
			} else {

				  if ((*i).is_bar()) {
					  mark.style = ArdourCanvas::Ruler::Mark::Major;
					  snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
				  } else {
					  mark.style = ArdourCanvas::Ruler::Mark::Minor;
					  snprintf (buf, sizeof(buf), "%" PRIu32, (*i).beat);
				  }
				  if (((*i).sample < bbt_position_of_helper) && helper_active) {
					  buf[0] = '\0';
				  }
				  mark.label =  buf;
				  mark.position = (*i).sample;
				  marks.push_back (mark);
				  n++;
			}

			/* Add the tick marks */
			skip = Temporal::ticks_per_beat / bbt_beat_subdivision;

			next_beat.beats = (*i).beat;
			next_beat.bars = (*i).bar;
			tick = skip; // the first non-beat tick
			t = 0;
			while (tick < Temporal::ticks_per_beat && (n < bbt_nmarks)) {

				next_beat.ticks = tick;
				pos = _session->tempo_map().sample_at_bbt (next_beat);
				if (t % bbt_accent_modulo == (bbt_accent_modulo - 1)) {
					i_am_accented = true;
				}

				if (pos > bbt_position_of_helper) {
					snprintf (buf, sizeof(buf), "%" PRIu32, tick);
				} else {
					buf[0] = '\0';
				}

				mark.label = buf;
				mark.position = pos;

				if ((bbt_beat_subdivision > 4) && i_am_accented) {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Micro;
				}
				i_am_accented = false;
				marks.push_back (mark);

				tick += skip;
				++t;
				++n;
			}
		}

	  break;

	case bbt_show_many:
		bbt_nmarks = 1;
		snprintf (buf, sizeof(buf), "cannot handle %" PRIu32 " bars", bbt_bars);
		mark.style = ArdourCanvas::Ruler::Mark::Major;
		mark.label = buf;
		mark.position = lower;
		marks.push_back (mark);
		break;

	case bbt_show_64:
			bbt_nmarks = (gint) (bbt_bars / 64) + 1;
			for (n = 0, i = grid.begin(); i != grid.end() && n < bbt_nmarks; i++) {
				if ((*i).is_bar()) {
					if ((*i).bar % 64 == 1) {
						if ((*i).bar % 256 == 1) {
							snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
							mark.style = ArdourCanvas::Ruler::Mark::Major;
						} else {
							buf[0] = '\0';
							if ((*i).bar % 256 == 129)  {
								mark.style = ArdourCanvas::Ruler::Mark::Minor;
							} else {
								mark.style = ArdourCanvas::Ruler::Mark::Micro;
							}
						}
						mark.label = buf;
						mark.position = (*i).sample;
						marks.push_back (mark);
						++n;
					}
				}
			}
			break;

	case bbt_show_16:
		bbt_nmarks = (bbt_bars / 16) + 1;
		for (n = 0,  i = grid.begin(); i != grid.end() && n < bbt_nmarks; i++) {
			if ((*i).is_bar()) {
			  if ((*i).bar % 16 == 1) {
				if ((*i).bar % 64 == 1) {
					snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					buf[0] = '\0';
					if ((*i).bar % 64 == 33)  {
						mark.style = ArdourCanvas::Ruler::Mark::Minor;
					} else {
						mark.style = ArdourCanvas::Ruler::Mark::Micro;
					}
				}
				mark.label = buf;
				mark.position = (*i).sample;
				marks.push_back (mark);
				++n;
			  }
			}
		}
	  break;

	case bbt_show_4:
		bbt_nmarks = (bbt_bars / 4) + 1;
		for (n = 0, i = grid.begin(); i != grid.end() && n < bbt_nmarks; ++i) {
			if ((*i).is_bar()) {
				if ((*i).bar % 4 == 1) {
					if ((*i).bar % 16 == 1) {
						snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bar);
						mark.style = ArdourCanvas::Ruler::Mark::Major;
					} else {
						buf[0] = '\0';
						if ((*i).bar % 16 == 9)  {
							mark.style = ArdourCanvas::Ruler::Mark::Minor;
						} else {
							mark.style = ArdourCanvas::Ruler::Mark::Micro;
						}
					}
					mark.label = buf;
					mark.position = (*i).sample;
					marks.push_back (mark);
					++n;
				}
			}
		}
	  break;

	case bbt_show_1:
//	default:
		bbt_nmarks = bbt_bars + 2;
		for (n = 0,  i = grid.begin(); i != grid.end() && n < bbt_nmarks; ++i) {
			if ((*i).is_bar()) {
				snprintf (buf, sizeof(buf), "%" PRIu32, (*i).bbt().bars);
				mark.style = ArdourCanvas::Ruler::Mark::Major;
				mark.label = buf;
				mark.position = (*i).sample;
				marks.push_back (mark);
				++n;
			}
		}
		break;

	}
}

void
Editor::set_samples_ruler_scale (samplepos_t lower, samplepos_t upper)
{
	_samples_ruler_interval = (upper - lower) / 5;
}

void
Editor::metric_get_samples (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t /*upper*/, gint /*maxchars*/)
{
	samplepos_t pos;
	samplepos_t const ilower = (samplepos_t) floor (lower);
	gchar buf[16];
	gint nmarks;
	gint n;
	ArdourCanvas::Ruler::Mark mark;

	if (_session == 0) {
		return;
	}

	nmarks = 5;
	for (n = 0, pos = ilower; n < nmarks; pos += _samples_ruler_interval, ++n) {
		snprintf (buf, sizeof(buf), "%" PRIi64, pos);
		mark.label = buf;
		mark.position = pos;
		mark.style = ArdourCanvas::Ruler::Mark::Major;
		marks.push_back (mark);
	}
}

static void
sample_to_clock_parts (samplepos_t sample,
                       samplepos_t sample_rate,
                       long*       hrs_p,
                       long*       mins_p,
                       long*       secs_p,
                       long*       millisecs_p)
{
	samplepos_t left;
	long hrs;
	long mins;
	long secs;
	long millisecs;

	left = sample;
	hrs = left / (sample_rate * 60 * 60 * 1000);
	left -= hrs * sample_rate * 60 * 60 * 1000;
	mins = left / (sample_rate * 60 * 1000);
	left -= mins * sample_rate * 60 * 1000;
	secs = left / (sample_rate * 1000);
	left -= secs * sample_rate * 1000;
	millisecs = left / sample_rate;

	*millisecs_p = millisecs;
	*secs_p = secs;
	*mins_p = mins;
	*hrs_p = hrs;

	return;
}

void
Editor::set_minsec_ruler_scale (samplepos_t lower, samplepos_t upper)
{
	samplepos_t fr = _session->sample_rate() * 1000;
	samplepos_t spacer;

	if (_session == 0) {
		return;
	}


	/* to prevent 'flashing' */
	if (lower > (spacer = (samplepos_t)(128 * Editor::get_current_zoom ()))) {
		lower -= spacer;
	} else {
		lower = 0;
	}
	upper += spacer;
	samplecnt_t const range = (upper - lower) * 1000;

	if (range <= (fr / 10)) { /* 0-0.1 second */
		minsec_mark_interval = fr / 1000; /* show 1/1000 seconds */
		minsec_ruler_scale = minsec_show_msecs;
		minsec_mark_modulo = 10;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= (fr / 2)) { /* 0-0.5 second */
		minsec_mark_interval = fr / 100;  /* show 1/100 seconds */
		minsec_ruler_scale = minsec_show_msecs;
		minsec_mark_modulo = 100;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= fr) { /* 0-1 second */
		minsec_mark_interval = fr / 10;  /* show 1/10 seconds */
		minsec_ruler_scale = minsec_show_msecs;
		minsec_mark_modulo = 200;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 2 * fr) { /* 1-2 seconds */
		minsec_mark_interval = fr / 10; /* show 1/10 seconds */
		minsec_ruler_scale = minsec_show_msecs;
		minsec_mark_modulo = 500;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 8 * fr) { /* 2-5 seconds */
		minsec_mark_interval =  fr / 5; /* show 2 seconds */
		minsec_ruler_scale = minsec_show_msecs;
		minsec_mark_modulo = 1000;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 16 * fr) { /* 8-16 seconds */
		minsec_mark_interval =  fr; /* show 1 seconds */
		minsec_ruler_scale = minsec_show_seconds;
		minsec_mark_modulo = 2;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 30 * fr) { /* 10-30 seconds */
		minsec_mark_interval =  fr; /* show 1 seconds */
		minsec_ruler_scale = minsec_show_seconds;
		minsec_mark_modulo = 5;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 60 * fr) { /* 30-60 seconds */
		minsec_mark_interval = fr; /* show 1 seconds */
		minsec_ruler_scale = minsec_show_seconds;
		minsec_mark_modulo = 5;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 2 * 60 * fr) { /* 1-2 minutes */
		minsec_mark_interval = 5 * fr; /* show 5 seconds */
		minsec_ruler_scale = minsec_show_seconds;
		minsec_mark_modulo = 3;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 4 * 60 * fr) { /* 4 minutes */
		minsec_mark_interval = 5 * fr; /* show 10 seconds */
		minsec_ruler_scale = minsec_show_seconds;
		minsec_mark_modulo = 30;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 10 * 60 * fr) { /* 10 minutes */
		minsec_mark_interval = 30 * fr; /* show 30 seconds */
		minsec_ruler_scale = minsec_show_seconds;
		minsec_mark_modulo = 120;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 30 * 60 * fr) { /* 10-30 minutes */
		minsec_mark_interval =  60 * fr; /* show 1 minute */
		minsec_ruler_scale = minsec_show_minutes;
		minsec_mark_modulo = 5;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 60 * 60 * fr) { /* 30 minutes - 1hr */
		minsec_mark_interval = 2 * 60 * fr; /* show 2 minutes */
		minsec_ruler_scale = minsec_show_minutes;
		minsec_mark_modulo = 10;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 4 * 60 * 60 * fr) { /* 1 - 4 hrs*/
		minsec_mark_interval = 5 * 60 * fr; /* show 10 minutes */
		minsec_ruler_scale = minsec_show_minutes;
		minsec_mark_modulo = 30;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 8 * 60 * 60 * fr) { /* 4 - 8 hrs*/
		minsec_mark_interval = 20 * 60 * fr; /* show 20 minutes */
		minsec_ruler_scale = minsec_show_minutes;
		minsec_mark_modulo = 60;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else if (range <= 16 * 60 * 60 * fr) { /* 16-24 hrs*/
		minsec_mark_interval =  60 * 60 * fr; /* show 60 minutes */
		minsec_ruler_scale = minsec_show_hours;
		minsec_mark_modulo = 2;
		minsec_nmarks = 2 + (range / minsec_mark_interval);
	} else {

		const samplecnt_t hours_in_range = range / (60 * 60 * fr);
		const int text_width_rough_guess = 70; /* pixels, very very approximate guess at how wide the tick mark text is */

		/* Normally we do not need to know anything about the width of the canvas
		   to set the ruler scale, because the caller has already determined
		   the width and set lower + upper arguments to this function to match that.

		   But in this case, where the range defined by lower and uppper can vary
		   substantially (anything from 24hrs+ to several billion years)
		   trying to decide which tick marks to show does require us to know
		   about the available width.
		*/

		minsec_nmarks = _track_canvas->width() / text_width_rough_guess;
		minsec_mark_modulo = std::max ((samplecnt_t) 1, 1 + (hours_in_range / minsec_nmarks));
		minsec_mark_interval = minsec_mark_modulo * (60 * 60 * fr);
		minsec_ruler_scale = minsec_show_many_hours;
	}
}

void
Editor::metric_get_minsec (std::vector<ArdourCanvas::Ruler::Mark>& marks, int64_t lower, int64_t upper, gint /*maxchars*/)
{
	samplepos_t pos;
	samplepos_t spacer;
	long hrs, mins, secs, millisecs;
	gchar buf[16];
	gint n;
	ArdourCanvas::Ruler::Mark mark;

	if (_session == 0) {
		return;
	}

	/* to prevent 'flashing' */
	if (lower > (spacer = (samplepos_t) (128 * Editor::get_current_zoom ()))) {
		lower = lower - spacer;
	} else {
		lower = 0;
	}

	if (minsec_mark_interval == 0) {  //we got here too early; divide-by-zero imminent
		return;
	}

	pos = (((1000 * (samplepos_t) floor(lower)) + (minsec_mark_interval/2))/minsec_mark_interval) * minsec_mark_interval;

	switch (minsec_ruler_scale) {

	case minsec_show_msecs:
		for (n = 0; n < minsec_nmarks && n < upper; pos += minsec_mark_interval, ++n) {
			sample_to_clock_parts (pos, _session->sample_rate(), &hrs, &mins, &secs, &millisecs);
			if (millisecs % minsec_mark_modulo == 0) {
				if (millisecs == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
				}
				snprintf (buf, sizeof(buf), "%02ld:%02ld:%02ld.%03ld", hrs, mins, secs, millisecs);
			} else {
				buf[0] = '\0';
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
			}
			mark.label = buf;
			mark.position = pos/1000.0;
			marks.push_back (mark);
		}
		break;

	case minsec_show_seconds:
		for (n = 0; n < minsec_nmarks; pos += minsec_mark_interval, ++n) {
			sample_to_clock_parts (pos, _session->sample_rate(), &hrs, &mins, &secs, &millisecs);
			if (secs % minsec_mark_modulo == 0) {
				if (secs == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
				}
				snprintf (buf, sizeof(buf), "%02ld:%02ld:%02ld", hrs, mins, secs);
			} else {
				buf[0] = '\0';
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
			}
			mark.label = buf;
			mark.position = pos/1000.0;
			marks.push_back (mark);
		}
		break;

	case minsec_show_minutes:
		for (n = 0; n < minsec_nmarks; pos += minsec_mark_interval, ++n) {
			sample_to_clock_parts (pos, _session->sample_rate(), &hrs, &mins, &secs, &millisecs);
			if (mins % minsec_mark_modulo == 0) {
				if (mins == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
				}
				snprintf (buf, sizeof(buf), "%02ld:%02ld:%02ld", hrs, mins, secs);
			} else {
				buf[0] = '\0';
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
			}
			mark.label = buf;
			mark.position = pos/1000.0;
			marks.push_back (mark);
		}
		break;

	case minsec_show_hours:
		 for (n = 0; n < minsec_nmarks; pos += minsec_mark_interval, ++n) {
			sample_to_clock_parts (pos, _session->sample_rate(), &hrs, &mins, &secs, &millisecs);
			if (hrs % minsec_mark_modulo == 0) {
				mark.style = ArdourCanvas::Ruler::Mark::Major;
				snprintf (buf, sizeof(buf), "%02ld:%02ld", hrs, mins);
			} else {
				buf[0] = '\0';
				mark.style = ArdourCanvas::Ruler::Mark::Micro;
			}
			mark.label = buf;
			mark.position = pos/1000.0;
			marks.push_back (mark);
		 }
		 break;

	case minsec_show_many_hours:
		for (n = 0; n < minsec_nmarks;) {
			sample_to_clock_parts (pos, _session->sample_rate(), &hrs, &mins, &secs, &millisecs);
			if (hrs % minsec_mark_modulo == 0) {
				mark.style = ArdourCanvas::Ruler::Mark::Major;
				snprintf (buf, sizeof(buf), "%02ld:00", hrs);
				mark.label = buf;
				mark.position = pos/1000.0;
				marks.push_back (mark);
				++n;
			}
			pos += minsec_mark_interval;
		}
		break;
	}
}
