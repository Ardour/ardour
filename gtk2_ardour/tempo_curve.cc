#include <sigc++/bind.h>
#include "ardour/tempo.h"

#include "canvas/rectangle.h"
#include "canvas/container.h"
#include "canvas/curve.h"
#include "canvas/canvas.h"
#include "canvas/debug.h"

#include "ui_config.h"

#include "tempo_curve.h"
#include "public_editor.h"
#include "utils.h"
#include "rgb_macros.h"

#include <gtkmm2ext/utils.h>

#include "pbd/i18n.h"

PBD::Signal1<void,TempoCurve*> TempoCurve::CatchDeletion;

static double curve_height = 13.0;

void TempoCurve::setup_sizes(const double timebar_height)
{
	curve_height = floor (timebar_height) - 2.5;
}
/* ignores Tempo note type - only note_types_per_minute is potentially curved */
TempoCurve::TempoCurve (PublicEditor& ed, ArdourCanvas::Container& parent, guint32 rgba, ARDOUR::TempoSection& temp, framepos_t frame, bool handle_events)

	: editor (ed)
	, _parent (&parent)
	, _curve (0)
	, _shown (false)
	, _color (rgba)
	, _min_tempo (temp.note_types_per_minute())
	, _max_tempo (temp.note_types_per_minute())
	, _tempo (temp)

{
	frame_position = frame;
	unit_position = editor.sample_to_pixel (frame);

	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple (unit_position, 1));
#ifdef CANVAS_DEBUG
	group->name = string_compose ("TempoCurve::group for %1", _tempo.note_types_per_minute());
#endif

	_curve = new ArdourCanvas::FramedCurve (group);
#ifdef CANVAS_DEBUG
	_curve->name = string_compose ("TempoCurve::curve for %1", _tempo.note_types_per_minute());
#endif
	_curve->set_points_per_segment (3);
	points = new ArdourCanvas::Points ();
	_curve->set (*points);

	set_color_rgba (rgba);

	editor.ZoomChanged.connect (sigc::mem_fun (*this, &TempoCurve::reposition));

	/* events will be handled by both the group and the mark itself, so
	 * make sure they can both be used to lookup this object.
	 */

	_curve->set_data ("tempo curve", this);

	if (handle_events) {
		//group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
	}

	_curve->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_curve_event), _curve, this));

}

TempoCurve::~TempoCurve ()
{
	CatchDeletion (this); /* EMIT SIGNAL */

	/* destroying the parent group destroys its contents, namely any polygons etc. that we added */
	delete group;
}

void TempoCurve::reparent(ArdourCanvas::Container & parent)
{
	group->reparent (&parent);
	_parent = &parent;
}

void
TempoCurve::canvas_height_set (double h)
{
	_canvas_height = h;
}

ArdourCanvas::Item&
TempoCurve::the_item() const
{
	return *group;
}

void
TempoCurve::set_position (framepos_t frame, framepos_t end_frame)
{
	unit_position = editor.sample_to_pixel (frame);
	group->set_x_position (unit_position);
	frame_position = frame;
	_end_frame = end_frame;

	points->clear();
	points = new ArdourCanvas::Points ();

	points->push_back (ArdourCanvas::Duple (0.0, curve_height));

	if (frame >= end_frame) {
		/* shouldn't happen but ..*/
		const double tempo_at = _tempo.note_types_per_minute();
		const double y_pos =  (curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height);

		points->push_back (ArdourCanvas::Duple (0.0, y_pos));
		points->push_back (ArdourCanvas::Duple (1.0, y_pos));

	} else if (_tempo.type() == ARDOUR::TempoSection::Constant || _tempo.c() == 0.0) {
		const double tempo_at = _tempo.note_types_per_minute();
		const double y_pos =  (curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height);

		points->push_back (ArdourCanvas::Duple (0.0, y_pos));
		points->push_back (ArdourCanvas::Duple (editor.sample_to_pixel (end_frame - frame), y_pos));
	} else {

		const framepos_t frame_step = max ((end_frame - frame) / 5, (framepos_t) 1);
		framepos_t current_frame = frame;

		while (current_frame < end_frame) {
			const double tempo_at = _tempo.tempo_at_minute (_tempo.minute_at_frame (current_frame)).note_types_per_minute();
			const double y_pos = max ((curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height), 0.0);

			points->push_back (ArdourCanvas::Duple (editor.sample_to_pixel (current_frame - frame), min (y_pos, curve_height)));

			current_frame += frame_step;
		}

		const double tempo_at = _tempo.tempo_at_minute (_tempo.minute_at_frame (end_frame)).note_types_per_minute();
		const double y_pos = max ((curve_height) - (((tempo_at - _min_tempo) / (_max_tempo - _min_tempo)) * curve_height), 0.0);

		points->push_back (ArdourCanvas::Duple (editor.sample_to_pixel (end_frame - frame), min (y_pos, curve_height)));
	}

	_curve->set (*points);
}

void
TempoCurve::reposition ()
{
	set_position (frame_position, _end_frame);
}

void
TempoCurve::show ()
{
	_shown = true;

        group->show ();
}

void
TempoCurve::hide ()
{
	_shown = false;

	group->hide ();
}

void
TempoCurve::set_color_rgba (uint32_t c)
{
	_color = c;
	_curve->set_fill_color (UIConfiguration::instance().color_mod ("tempo curve", "selection rect"));
	_curve->set_outline_color (_color);

}
