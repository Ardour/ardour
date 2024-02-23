#include "cue_editor.h"

CueEditor::CueEditor (std::string const & name)
	: EditingContext (name)
{
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
CueEditor::select_all_within (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, TrackViewList const &, Selection::Operation, bool)
{
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

EditingContext::EnterContext*
CueEditor::get_enter_context(ItemType type)
{
	return nullptr;
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
CueEditor::set_zoom_focus (Editing::ZoomFocus)
{
}

Editing::ZoomFocus
CueEditor::get_zoom_focus () const
{
	return Editing::ZoomFocusPlayhead;
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
CueEditor::reposition_and_zoom (samplepos_t, double)
{
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

