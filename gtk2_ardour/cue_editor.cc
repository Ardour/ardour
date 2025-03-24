#include "cue_editor.h"
#include "editor_drag.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

CueEditor::CueEditor (std::string const & name)
	: EditingContext (name)
	, HistoryOwner (X_("cue-editor"))
{
	_history.Changed.connect (history_connection, invalidator (*this), std::bind (&CueEditor::history_changed, this), gui_context());

	set_zoom_focus (Editing::ZoomFocusLeft);
}

CueEditor::~CueEditor ()
{
}

void
CueEditor::set_snapped_cursor_position (Temporal::timepos_t const & pos)
{
}

std::vector<MidiRegionView*>
CueEditor::filter_to_unique_midi_region_views (RegionSelection const & ms) const
{
	std::vector<MidiRegionView*> mrv;
	return mrv;
}

void
CueEditor::get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const
{
}

StripableTimeAxisView*
CueEditor::get_stripable_time_axis_by_id (const PBD::ID& id) const
{
	return nullptr;
}

TrackViewList
CueEditor::axis_views_from_routes (std::shared_ptr<ARDOUR::RouteList>) const
{
	TrackViewList tvl;
	return tvl;
}

ARDOUR::Location*
CueEditor::find_location_from_marker (ArdourMarker*, bool&) const
{
	return nullptr;
}

ArdourMarker*
CueEditor::find_marker_from_location_id (PBD::ID const&, bool) const
{
	return nullptr;
}

TempoMarker*
CueEditor::find_marker_for_tempo (Temporal::TempoPoint const &)
{
	return nullptr;
}

MeterMarker*
CueEditor::find_marker_for_meter (Temporal::MeterPoint const &)
{
	return nullptr;
}

void
CueEditor::maybe_autoscroll (bool, bool, bool from_headers)
{
}

void
CueEditor::stop_canvas_autoscroll ()
{
}

bool
CueEditor::autoscroll_active() const
{
	return false;
}

void
CueEditor::redisplay_grid (bool immediate_redraw)
{
	update_grid ();
}

Temporal::timecnt_t
CueEditor::get_nudge_distance (Temporal::timepos_t const & pos, Temporal::timecnt_t& next) const
{
	return Temporal::timecnt_t (Temporal::AudioTime);
}

void
CueEditor::instant_save()
{
}

void
CueEditor::begin_selection_op_history ()
{
}

void
CueEditor::begin_reversible_selection_op (std::string cmd_name)
{
}

void
CueEditor::commit_reversible_selection_op ()
{
}

void
CueEditor::abort_reversible_selection_op ()
{
}

void
CueEditor::undo_selection_op ()
{
}

void
CueEditor::redo_selection_op ()
{
}

double
CueEditor::get_y_origin () const
{
	return 0.;
}

void
CueEditor::set_zoom_focus (Editing::ZoomFocus zf)
{
	using namespace Editing;

	/* We don't allow playhead for zoom focus here */

	if (zf == ZoomFocusPlayhead) {
		return;
	}

	std::string str = zoom_focus_strings[(int)zf];

	if (str != zoom_focus_selector.get_text()) {
		zoom_focus_selector.set_text (str);
	}

	if (_zoom_focus != zf) {
		_zoom_focus = zf;
		ZoomFocusChanged (); /* EMIT SIGNAL */
	}
}

void
CueEditor::set_samples_per_pixel (samplecnt_t n)
{
	samples_per_pixel = n;
	ZoomChanged(); /* EMIT SIGNAL */
}

samplecnt_t
CueEditor::get_current_zoom () const
{
	return samples_per_pixel;
}

void
CueEditor::reposition_and_zoom (samplepos_t pos, double spp)
{
	set_samples_per_pixel (spp);

	horizontal_adjustment.set_value (sample_to_pixel (pos));
	/* correct rounding errors */
	_leftmost_sample = pos;
}

void
CueEditor::set_mouse_mode (Editing::MouseMode, bool force)
{
}

void
CueEditor::step_mouse_mode (bool next)
{
}


void
CueEditor::reset_x_origin_to_follow_playhead ()
{
}


Gdk::Cursor*
CueEditor::get_canvas_cursor () const
{
	return nullptr;
}

Editing::MouseMode
CueEditor::current_mouse_mode () const
{
	return Editing::MouseContent;
}


std::shared_ptr<Temporal::TempoMap const>
CueEditor::start_local_tempo_map (std::shared_ptr<Temporal::TempoMap> map)
{
	std::shared_ptr<Temporal::TempoMap const> tmp = Temporal::TempoMap::use();
	Temporal::TempoMap::set (map);
	return tmp;
}

void
CueEditor::end_local_tempo_map (std::shared_ptr<Temporal::TempoMap const> map)
{
	Temporal::TempoMap::set (map);
}

void
CueEditor::do_undo (uint32_t n)
{
	if (_drags->active ()) {
		_drags->abort ();
	}

	_history.undo (n);
}

void
CueEditor::do_redo (uint32_t n)
{
	if (_drags->active ()) {
		_drags->abort ();
	}

	_history.redo (n);
}

void
CueEditor::history_changed ()
{
	update_undo_redo_actions (_history);
}

Temporal::timepos_t
CueEditor::_get_preferred_edit_position (Editing::EditIgnoreOption ignore, bool from_context_menu, bool from_outside_canvas)
{
	samplepos_t where;
	bool in_track_canvas = false;

	if (!mouse_sample (where, in_track_canvas)) {
		return Temporal::timepos_t (0);
	}

	return Temporal::timepos_t (where);
}
