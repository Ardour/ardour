/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013-2017 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2017 André Nusser <andre.nusser@googlemail.com>
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
#include <algorithm>
#include <ostream>

#include <ytkmm/ytkmm.h>

#include "gtkmm2ext/gtk_ui.h"

#include <sigc++/signal.h>

#include "midi++/midnam_patch.h"

#include "pbd/stateful_diff_command.h"
#include "pbd/unwind.h"

#include "ardour/debug.h"
#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/quantize.h"

#include "evoral/Parameter.h"
#include "evoral/Event.h"
#include "evoral/Control.h"
#include "evoral/midi_util.h"

#include "canvas/debug.h"

#include "automation_region_view.h"
#include "automation_time_axis.h"
#include "control_point.h"
#include "debug.h"
#include "editor.h"
#include "editor_drag.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "item_counts.h"
#include "keyboard.h"
#include "midi_channel_dialog.h"
#include "midi_cut_buffer.h"
#include "midi_list_editor.h"
#include "midi_region_view.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "midi_util.h"
#include "midi_velocity_dialog.h"
#include "note_player.h"
#include "paste_context.h"
#include "pianoroll_window.h"
#include "public_editor.h"
#include "route_time_axis.h"
#include "rgb_macros.h"
#include "selection.h"
#include "streamview.h"
#include "patch_change_dialog.h"
#include "velocity_ghost_region.h"
#include "verbose_cursor.h"
#include "note.h"
#include "hit.h"
#include "patch_change.h"
#include "sys_ex.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace std;
using namespace Temporal;
using Gtkmm2ext::Keyboard;

#define MIDI_BP_ZERO ((Config->get_first_midi_bank_is_zero())?0:1)

MidiRegionView::MidiRegionView (ArdourCanvas::Container*      parent,
                                EditingContext&               ec,
                                RouteTimeAxisView&            tv,
                                std::shared_ptr<MidiRegion>   r,
                                double                        spu,
                                uint32_t                      basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, MidiView (std::dynamic_pointer_cast<MidiTrack> (tv.stripable()), *group, ec, *dynamic_cast<MidiStreamView*>(tv.view()), basic_color)
{
	connect_to_diskstream ();
}

MidiRegionView::MidiRegionView (ArdourCanvas::Container*      parent,
                                EditingContext&               ec,
                                RouteTimeAxisView&            tv,
                                std::shared_ptr<MidiRegion>   r,
                                double                        spu,
                                uint32_t                      basic_color,
                                bool                          recording,
                                TimeAxisViewItem::Visibility  visibility)
	: RegionView (parent, tv, r, spu, basic_color, recording, visibility)
	, MidiView (std::dynamic_pointer_cast<MidiTrack> (tv.stripable()), *group, ec, *dynamic_cast<MidiStreamView*>(tv.view()), basic_color)
{
	connect_to_diskstream ();
}

MidiRegionView::MidiRegionView (const MidiRegionView& other)
	: sigc::trackable(other)
	, RegionView (other)
	, MidiView (other)
{
	init (false);
}

MidiRegionView::MidiRegionView (const MidiRegionView& other, std::shared_ptr<MidiRegion> region)
	: RegionView (other, std::shared_ptr<Region> (region))
	, MidiView (other)
{
	init (true);
}

void
MidiRegionView::init (bool /*wfd*/)
{
	DisplaySuspender ds (*this, true);

	RegionView::init (false);

	CANVAS_DEBUG_NAME (_note_group, string_compose ("note group for %1", get_item_name()));

	set_region (std::dynamic_pointer_cast<MidiRegion> (_region));

	//set_height (trackview.current_height());

	region_muted ();
	region_sync_changed ();
	region_resized (ARDOUR::bounds_change);
	//region_locked ();

	set_colors ();
	reset_width_dependent_items (_pixel_width);

	_note_group->parent()->raise_to_top();

	Config->ParameterChanged.connect (*this, invalidator (*this), std::bind (&MidiRegionView::parameter_changed, this, _1), gui_context());
	connect_to_diskstream ();
}

void
MidiRegionView::set_model (std::shared_ptr<ARDOUR::MidiModel> model)
{
	MidiView::set_model (model);

	region_muted ();
	region_sync_changed ();
	region_resized (ARDOUR::bounds_change);
	region_locked ();

	set_colors ();
	reset_width_dependent_items (_pixel_width);
}

bool
MidiRegionView::display_is_enabled () const
{
	return RegionView::display_enabled ();
}

void
MidiRegionView::parameter_changed (std::string const & p)
{
	RegionView::parameter_changed (p);

	if (p == "display-first-midi-bank-as-zero") {
		if (display_enabled()) {
			view_changed ();
		}
	} else if (p == "color-regions-using-track-color") {
		set_colors ();
	} else if (p == "use-note-color-for-velocity") {
		color_handler ();
	}
}

void
MidiRegionView::color_handler ()
{
	RegionView::color_handler ();
	MidiView::color_handler ();
}

void
MidiRegionView::region_resized (PBD::PropertyChange const & change)
{
	RegionView::region_resized (change);
	MidiView::region_resized (change);
}

InstrumentInfo&
MidiRegionView::instrument_info () const
{
	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);
	return route_ui->route()->instrument_info();
}

const std::shared_ptr<ARDOUR::MidiRegion>
MidiRegionView::midi_region() const
{
	return _midi_region;
}

void
MidiRegionView::connect_to_diskstream ()
{
	midi_view()->midi_track()->DataRecorded.connect(
		*this, invalidator(*this),
		std::bind (&MidiRegionView::data_recorded, this, _1),
		gui_context());
}

std::string
MidiRegionView::get_modifier_name () const
{
	const bool opaque = _region->opaque() || trackview.layer_display () == Stacked;

	std::string mod_name;

	if (_dragging) {
		mod_name = "dragging region";
	} else if (_editing_context.internal_editing()) {
		if (!opaque || _region->muted ()) {
			mod_name = "editable region";
		}
	} else {
		if (!opaque || _region->muted ()) {
			mod_name = "transparent region base";
		}
	}

	return mod_name;
}

GhostRegion*
MidiRegionView::add_ghost (TimeAxisView& tv)
{
	double unit_position = _editing_context.time_to_pixel (_region->position ());
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&tv);
	MidiGhostRegion* ghost;

	if (mtv && mtv->midi_view()) {
		return 0;
	} else {
		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*>(&tv);
		if (atv && atv->parameter() == Evoral::Parameter (MidiVelocityAutomation)) {
			ghost = new VelocityGhostRegion (*this, tv, trackview, unit_position);
		} else {
			ghost = new MidiGhostRegion (*this, tv, trackview, unit_position);
		}
	}

	ghost->set_colors ();
	ghost->set_height ();
	ghost->set_duration (_region->length().samples() / samples_per_pixel);

	std::cerr << "Adding " << _events.size() << " notes to ghost\n";
	for (auto const & i : _events) {
		ghost->add_note (i.second);
	}

	ghosts.push_back (ghost);
	return ghost;
}


bool
MidiRegionView::canvas_group_event(GdkEvent* ev)
{
	if (in_destructor || _recregion) {
		return false;
	}

	if (!_editing_context.internal_editing()) {
		// not in internal edit mode, so just act like a normal region
		return RegionView::canvas_group_event (ev);
	}

	return MidiView::midi_canvas_group_event (ev);
}

bool
MidiRegionView::enter_notify (GdkEventCrossing* ev)
{
	enter_internal (ev->state);

	_entered = true;
	return false;
}

bool
MidiRegionView::leave_notify (GdkEventCrossing*)
{
	leave_internal ();

	_entered = false;
	return false;
}

void
MidiRegionView::mouse_mode_changed ()
{
	// Adjust frame colour (become more transparent for internal tools)
	set_frame_color();
	MidiView::mouse_mode_changed ();
}

void
MidiRegionView::enter_internal (uint32_t state)
{
	if (_editing_context.current_mouse_mode() == MouseDraw && !draw_drag) {
		// Show ghost note under pencil
		create_ghost_note(_last_event_x, _last_event_y, state);
	}

	// Lower frame handles below notes so they don't steal events

	if (frame_handle_start) {
		frame_handle_start->lower_to_bottom();
	}
	if (frame_handle_end) {
		frame_handle_end->lower_to_bottom();
	}
}

void
MidiRegionView::leave_internal()
{
	hide_verbose_cursor ();
	remove_ghost_note ();
	_entered_note = 0;

	// Raise frame handles above notes so they catch events
	if (frame_handle_start) {
		frame_handle_start->raise_to_top();
	}

	if (frame_handle_end) {
		frame_handle_end->raise_to_top();
	}
}

bool
MidiRegionView::motion (GdkEventMotion* ev)
{
	MidiView::motion (ev);
	return RegionView::canvas_group_event ((GdkEvent *) ev);
}


bool
MidiRegionView::scroll (GdkEventScroll* ev)
{
	if (_editing_context.drags()->active()) {
		return false;
	}

	if (!_editing_context.get_selection().selected (this)) {
		return false;
	}

	if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier) ||
	    Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier)) {
		/* XXX: bit of a hack; allow PrimaryModifier+TertiaryModifier scroll
		 * through so that it still works for navigation and zoom.
		 */
		return false;
	}

	if (_selection.empty()) {
		const int step = 1;
		const bool zoom = Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier);
		const bool just_one_edge = Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier|Keyboard::PrimaryModifier);

		switch (ev->direction) {
		case GDK_SCROLL_UP:
			if (just_one_edge) {
				/* make higher notes visible aka expand higher pitch range */
				midi_stream_view()->apply_note_range (midi_stream_view()->lowest_note(), min (127, midi_stream_view()->highest_note() + step), true);
			} else if (zoom) {
				/* zoom out to show more higher and lower pitches */
				midi_stream_view()->apply_note_range (max (0, midi_stream_view()->lowest_note() - step), min (127, midi_stream_view()->highest_note() + step), true);
			} else {
				/* scroll towards higher pitches */
				midi_stream_view()->apply_note_range (max (0, midi_stream_view()->lowest_note() + step), min (127, midi_stream_view()->highest_note() + step), true);
			}
			return true;

		case GDK_SCROLL_DOWN:
			if (just_one_edge) {
				/* make lower notes visible aka expand lower pitch range */
				midi_stream_view()->apply_note_range (max (0, midi_stream_view()->lowest_note() - step), midi_stream_view()->highest_note(), true);
			} else if (zoom) {
				/* zoom in to show less higher and lower pitches */
				midi_stream_view()->apply_note_range (min (127, midi_stream_view()->lowest_note() + step), max (0, midi_stream_view()->highest_note() - step), true);
			} else {
				/* scroll towards lower pitches */
				midi_stream_view()->apply_note_range (min (127, midi_stream_view()->lowest_note() - step), max (0, midi_stream_view()->highest_note() - step), true);
			}
			return true;

		default:
			break;
		}

		return false;
	}

	hide_verbose_cursor ();

	if (UIConfiguration::instance().get_scroll_velocity_editing()) {
		bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
		Keyboard::ModifierMask mask_together(Keyboard::PrimaryModifier|Keyboard::TertiaryModifier);
		bool together = Keyboard::modifier_state_contains (ev->state, mask_together);

		if (ev->direction == GDK_SCROLL_UP) {
			change_velocities (true, fine, false, together);
		} else if (ev->direction == GDK_SCROLL_DOWN) {
			change_velocities (false, fine, false, together);
		} else {
			/* left, right: we don't use them */
			return false;
		}

		return true;
	}

	return false;
}

void
MidiRegionView::ghosts_view_changed ()
{
	for (auto & g : ghosts) {
		MidiGhostRegion* gr = dynamic_cast<MidiGhostRegion*> (g);
		if (gr && !gr->trackview.hidden()) {
			gr->view_changed ();
		}
	}
}

void
MidiRegionView::clear_ghost_events()
{
	for (auto & ghost : ghosts) {

		MidiGhostRegion* gr;

		if ((gr = dynamic_cast<MidiGhostRegion*>(ghost)) != 0) {
			gr->clear_events ();
		}
	}
}

void
MidiRegionView::ghosts_model_changed()
{
	for (auto & ghost : ghosts) {

		MidiGhostRegion* gr;

		if ((gr = dynamic_cast<MidiGhostRegion*>(ghost)) != 0) {
			if (!gr->trackview.hidden()) {
				gr->model_changed ();
			}
		}
	}
}

void
MidiRegionView::ghost_remove_note (NoteBase* nb)
{
	for (auto & ghost : ghosts) {

		MidiGhostRegion* gr;

		if ((gr = dynamic_cast<MidiGhostRegion*>(ghost)) != 0) {
			gr->remove_note (nb);
		}
	}
}

void
MidiRegionView::ghost_add_note (NoteBase* nb)
{
	for (auto & ghost : ghosts) {

		MidiGhostRegion* gr;

		std::cerr << "GAN on " << ghost << std::endl;

		if ((gr = dynamic_cast<MidiGhostRegion*>(ghost)) != 0) {
			gr->add_note (nb);
		}
	}
}

void
MidiRegionView::ghost_sync_selection (NoteBase* nb)
{
	for (auto & ghost : ghosts) {

		MidiGhostRegion* gr;

		if ((gr = dynamic_cast<MidiGhostRegion*>(ghost)) != 0) {
			gr->note_selected (nb);
		}
	}
}

MidiRegionView::~MidiRegionView ()
{
	in_destructor = true;
	RegionViewGoingAway (this); /* EMIT_SIGNAL */
}

void
MidiRegionView::reset_width_dependent_items (double pixel_width)
{
	MidiView::reset_width_dependent_items (pixel_width);
	RegionView::reset_width_dependent_items (pixel_width);
}

void
MidiRegionView::set_height (double height)
{
	MidiView::set_height (height);
	RegionView::set_height(height);
}
void
MidiRegionView::set_selected (bool selected)
{
	if (!selected) {
		clear_selection_internal ();
	}

	RegionView::set_selected (selected);
}

uint32_t
MidiRegionView::get_fill_color() const
{
	Gtkmm2ext::Color c;
	if (_selected) {
		c = UIConfiguration::instance().color ("selected region base");
	} else if ((!UIConfiguration::instance().get_show_name_highlight() || high_enough_for_name) && !UIConfiguration::instance().get_color_regions_using_track_color()) {
		c = UIConfiguration::instance().color (fill_color_name);
	} else {
		c = fill_color;
	}

	string mod_name = get_modifier_name();

	if (mod_name.empty ()) {
		return c;
	} else {
		return UIConfiguration::instance().color_mod (c, mod_name);
	}
}

double
MidiRegionView::height() const
{
	return TimeAxisViewItem::height();
}

void
MidiRegionView::redisplay (bool view_only)
{
	MidiView::redisplay (view_only);
}

ArdourCanvas::Item*
MidiRegionView::drag_group () const
{
	return get_canvas_group ();
}

void
MidiRegionView::select_self (bool  add)
{
	if (add) {
		_editing_context.get_selection().add (this);
	} else {
		_editing_context.get_selection().set (this);
	}
}

void
MidiRegionView::unselect_self ()
{
	_editing_context.get_selection().remove (this);
}

void
MidiRegionView::begin_drag_edit (std::string const & why)
{
	if (!_selected) {
		/* unclear why gcc can't understand which version of
		   select_self() to use here, but so be it.
		*/
		MidiView::select_self ();
	}
	// start_note_diff_command (why);
}

void
MidiRegionView::select_self_uniquely ()
{
	_editing_context.set_selected_midi_region_view (*this);
}

void
MidiRegionView::set_visibility_note_range (MidiViewBackground::VisibleNoteRange vnr, bool from_selection)
{
	dynamic_cast<MidiTimeAxisView*>(&trackview)->set_visibility_note_range (vnr, from_selection);
}

MergeableLine*
MidiRegionView::make_merger ()
{
	return nullptr;
}

void
MidiRegionView::add_control_points_to_selection (timepos_t const & start, timepos_t const & end, double gy0, double gy1)
{
	typedef RouteTimeAxisView::AutomationTracks ATracks;
	typedef std::list<Selectable*>              Selectables;

	const ATracks& atracks = dynamic_cast<StripableTimeAxisView*>(&trackview)->automation_tracks();
	Selectables    selectables;
	_editing_context.get_selection().clear_points();

	timepos_t st (start);
	timepos_t et (end);

	for (auto const & at : atracks) {

		at.second->get_selectables (st, et, gy0, gy1, selectables);

		for (Selectables::const_iterator s = selectables.begin(); s != selectables.end(); ++s) {
			ControlPoint* cp = dynamic_cast<ControlPoint*>(*s);
			if (cp) {
				_editing_context.get_selection().add(cp);
			}
		}

		at.second->set_selected_points (_editing_context.get_selection().points);
	}
}

void
MidiRegionView::edit_in_pianoroll_window ()
{
	std::shared_ptr<MidiTrack> track = std::dynamic_pointer_cast<MidiTrack> (trackview.stripable());
	assert (track);

	PianorollWindow* pr = new PianorollWindow (string_compose (_("Pianoroll: %1"), _region->name()), track->session());;

	pr->set (track, midi_region());
	pr->show_all ();
	pr->present ();

	pr->signal_delete_event().connect (sigc::mem_fun (*this, &MidiRegionView::pianoroll_window_deleted), false);
	_editor = pr;
}

bool
MidiRegionView::pianoroll_window_deleted (GdkEventAny*)
{
	_editor = nullptr;
	return false;
}

void
MidiRegionView::show_region_editor ()
{
	edit_in_pianoroll_window ();
}

void
MidiRegionView::hide_region_editor ()
{
	RegionView::hide_region_editor ();
	delete _editor;
	_editor = nullptr;
}
