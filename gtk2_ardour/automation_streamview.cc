/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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
#include <list>
#include <utility>

#include <gtkmm.h>

#include "gtkmm2ext/gtk_ui.h"

#include "pbd/compose.h"
#include "canvas/debug.h"

#include "ardour/midi_region.h"
#include "ardour/midi_source.h"

#include "automation_region_view.h"
#include "automation_streamview.h"
#include "automation_time_axis.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "region_selection.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;

AutomationStreamView::AutomationStreamView (AutomationTimeAxisView& tv)
	: StreamView (*dynamic_cast<RouteTimeAxisView*>(tv.get_parent()),
	              tv.canvas_display())
	, _automation_view(tv)
	, _pending_automation_state (Off)
{
	CANVAS_DEBUG_NAME (_canvas_group, string_compose ("SV canvas group auto %1", tv.name()));
	CANVAS_DEBUG_NAME (canvas_rect, string_compose ("SV canvas rectangle auto %1", tv.name()));

	color_handler ();

	UIConfiguration::instance().ColorsChanged.connect(sigc::mem_fun(*this, &AutomationStreamView::color_handler));
}

AutomationStreamView::~AutomationStreamView ()
{
}


RegionView*
AutomationStreamView::add_region_view_internal (boost::shared_ptr<Region> region, bool wait_for_data, bool /*recording*/)
{
	if (!region) {
		return 0;
	}

	if (wait_for_data) {
		boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(region);
		if (mr) {
			Source::Lock lock(mr->midi_source()->mutex());
			mr->midi_source()->load_model(lock);
		}
	}

	const boost::shared_ptr<AutomationControl> control = boost::dynamic_pointer_cast<AutomationControl> (
		region->control (_automation_view.parameter(), true)
		);

	boost::shared_ptr<AutomationList> list;
	if (control) {
		list = boost::dynamic_pointer_cast<AutomationList>(control->list());
		if (control->list() && !list) {
			error << _("unable to display automation region for control without list") << endmsg;
			return 0;
		}
	}

	AutomationRegionView *region_view;
	std::list<RegionView *>::iterator i;

	for (i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region() == region) {

			/* great. we already have an AutomationRegionView for this Region. use it again. */
			AutomationRegionView* arv = dynamic_cast<AutomationRegionView*>(*i);;

			if (arv->line()) {
				arv->line()->set_list (list);
			}
			(*i)->set_valid (true);
			(*i)->enable_display (wait_for_data);
			display_region(arv);

			return 0;
		}
	}

	region_view = new AutomationRegionView (
		_canvas_group, _automation_view, region,
		_automation_view.parameter (), list,
		_samples_per_pixel, region_color
		);

	region_view->init (false);
	region_views.push_front (region_view);

	/* follow global waveform setting */

	if (wait_for_data) {
		region_view->enable_display(true);
		// region_view->midi_region()->midi_source(0)->load_model();
	}

	display_region (region_view);

	/* catch regionview going away */
	region->DropReferences.connect (*this, invalidator (*this), boost::bind (&AutomationStreamView::remove_region_view, this, boost::weak_ptr<Region>(region)), gui_context());

	/* setup automation state for this region */
	boost::shared_ptr<AutomationLine> line = region_view->line ();
	if (line && line->the_list()) {
		line->the_list()->set_automation_state (automation_state ());
	}

	RegionViewAdded (region_view);

	return region_view;
}

void
AutomationStreamView::display_region(AutomationRegionView* region_view)
{
	region_view->line().reset();
}

void
AutomationStreamView::set_automation_state (AutoState state)
{
	/* Setting the automation state for this view sets the state of all regions' lists to the same thing */

	if (region_views.empty()) {
		_pending_automation_state = state;
	} else {
		list<boost::shared_ptr<AutomationLine> > lines = get_lines ();

		for (list<boost::shared_ptr<AutomationLine> >::iterator i = lines.begin(); i != lines.end(); ++i) {
			if ((*i)->the_list()) {
				(*i)->the_list()->set_automation_state (state);
			}
		}
	}
}

void
AutomationStreamView::redisplay_track ()
{
	// Flag region views as invalid and disable drawing
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		(*i)->set_valid (false);
		(*i)->enable_display(false);
	}

	// Add and display region views, and flag them as valid
	if (_trackview.is_track()) {
		_trackview.track()->playlist()->foreach_region (
			sigc::hide_return (sigc::mem_fun (*this, &StreamView::add_region_view))
			);
	}

	// Stack regions by layer, and remove invalid regions
	layer_regions();
}


void
AutomationStreamView::setup_rec_box ()
{
}

void
AutomationStreamView::color_handler ()
{
	if (_trackview.is_midi_track()) {
		canvas_rect->set_fill_color (UIConfiguration::instance().color_mod ("midi track base", "midi track base"));
	} else {
		canvas_rect->set_fill_color (UIConfiguration::instance().color ("midi bus base"));
	}
}

AutoState
AutomationStreamView::automation_state () const
{
	if (region_views.empty()) {
		return _pending_automation_state;
	}

	boost::shared_ptr<AutomationLine> line = ((AutomationRegionView*) region_views.front())->line ();
	if (!line || !line->the_list()) {
		return Off;
	}

	return line->the_list()->automation_state ();
}

bool
AutomationStreamView::has_automation () const
{
	list<boost::shared_ptr<AutomationLine> > lines = get_lines ();

	for (list<boost::shared_ptr<AutomationLine> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		if ((*i)->npoints() > 0) {
			return true;
		}
	}

	return false;
}

/** Our parent AutomationTimeAxisView calls this when the user requests a particular
 *  InterpolationStyle; tell the AutomationLists in our regions.
 */
void
AutomationStreamView::set_interpolation (AutomationList::InterpolationStyle s)
{
	list<boost::shared_ptr<AutomationLine> > lines = get_lines ();

	for (list<boost::shared_ptr<AutomationLine> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->the_list()->set_interpolation (s);
	}
}

AutomationList::InterpolationStyle
AutomationStreamView::interpolation () const
{
	if (region_views.empty()) {
		return AutomationList::Linear;
	}

	AutomationRegionView* v = dynamic_cast<AutomationRegionView*> (region_views.front());
	if (v) {
		return v->line()->the_list()->interpolation ();
	}
	return AutomationList::Linear;
}

/** Clear all automation displayed in this view */
void
AutomationStreamView::clear ()
{
	list<boost::shared_ptr<AutomationLine> > lines = get_lines ();

	for (list<boost::shared_ptr<AutomationLine> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->clear ();
	}
}

/** @param start Start position in session samples.
 *  @param end End position in session samples.
 *  @param bot Bottom position expressed as a fraction of track height where 0 is the bottom of the track.
 *  @param top Top position expressed as a fraction of track height where 0 is the bottom of the track.
 *  NOTE: this y system is different to that for the StreamView method that this overrides, which is a little
 *  confusing.
 */
void
AutomationStreamView::get_selectables (timepos_t const & start, timepos_t const & end, double botfrac, double topfrac, list<Selectable*>& results, bool /*within*/)
{
	for (list<RegionView*>::iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AutomationRegionView* arv = dynamic_cast<AutomationRegionView*> (*i);
		if (arv) {
			arv->line()->get_selectables (start, end, botfrac, topfrac, results);
		}
	}
}

void
AutomationStreamView::set_selected_points (PointSelection& ps)
{
	list<boost::shared_ptr<AutomationLine> > lines = get_lines ();

	for (list<boost::shared_ptr<AutomationLine> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->set_selected_points (ps);
	}
}

list<boost::shared_ptr<AutomationLine> >
AutomationStreamView::get_lines () const
{
	list<boost::shared_ptr<AutomationLine> > lines;

	for (list<RegionView*>::const_iterator i = region_views.begin(); i != region_views.end(); ++i) {
		AutomationRegionView* arv = dynamic_cast<AutomationRegionView*> (*i);
		if (arv) {
			lines.push_back (arv->line());
		}
	}

	return lines;
}

bool
AutomationStreamView::paste (timepos_t const &                         pos,
                             unsigned                                  paste_count,
                             float                                     times,
                             boost::shared_ptr<ARDOUR::AutomationList> alist)
{
	/* XXX: not sure how best to pick this; for now, just use the last region which starts before pos */

	if (region_views.empty()) {
		return false;
	}

	region_views.sort (RegionView::PositionOrder());

	list<RegionView*>::const_iterator prev = region_views.begin ();

	for (list<RegionView*>::const_iterator i = region_views.begin(); i != region_views.end(); ++i) {
		if ((*i)->region()->nt_position() > pos) {
			break;
		}
		prev = i;
	}

	boost::shared_ptr<Region> r = (*prev)->region ();

	/* If *prev doesn't cover pos, it's no good */
	if (r->nt_position() > pos || ((r->nt_position() + r->nt_length()) < pos)) {
		return false;
	}

	AutomationRegionView* arv = dynamic_cast<AutomationRegionView*> (*prev);
	return arv ? arv->paste(pos, paste_count, times, alist) : false;
}
