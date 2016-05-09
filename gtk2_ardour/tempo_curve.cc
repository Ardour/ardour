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

#include "i18n.h"

PBD::Signal1<void,TempoCurve*> TempoCurve::CatchDeletion;

static double curve_height = 13.0;

void TempoCurve::setup_sizes(const double timebar_height)
{
	curve_height = floor (timebar_height) - 2;
}

TempoCurve::TempoCurve (PublicEditor& ed, ArdourCanvas::Container& parent, guint32 rgba, ARDOUR::TempoSection& temp, framepos_t frame, bool handle_events)

	: editor (ed)
	, _parent (&parent)
	, _curve (0)
	, _shown (false)
	, _color (rgba)
	, _min_tempo (temp.beats_per_minute())
	, _max_tempo (temp.beats_per_minute())
	, _tempo (temp)


{

	points = new ArdourCanvas::Points ();
	points->push_back (ArdourCanvas::Duple (0.0, 0.0));
	points->push_back (ArdourCanvas::Duple (1.0, 0.0));
	points->push_back (ArdourCanvas::Duple (1.0, curve_height));
	points->push_back (ArdourCanvas::Duple (0.0, curve_height));

	frame_position = frame;
	unit_position = editor.sample_to_pixel (frame);

	group = new ArdourCanvas::Container (&parent, ArdourCanvas::Duple (unit_position, 0));
#ifdef CANVAS_DEBUG
	group->name = string_compose ("Marker::group for %1", _tempo.beats_per_minute());
#endif

	_background = new ArdourCanvas::Rectangle (group);
#ifdef CANVAS_DEBUG
	_background->name = string_compose ("TempoCurve::_background for %1", _tempo.beats_per_minute());
#endif
	_background->set_x0 (0.0);
	_background->set_x1 (ArdourCanvas::COORD_MAX);
	_background->set_outline_what (ArdourCanvas::Rectangle::What(0));
	_curve = new ArdourCanvas::Curve (group);
#ifdef CANVAS_DEBUG
	_curve->name = string_compose ("TempoCurve::_curve for %1", _tempo.beats_per_minute());
#endif
	_curve->set_fill_mode (ArdourCanvas::Curve::Inside);
	_curve->set_points_per_segment (32);
	_curve->set (*points);

	set_color_rgba (rgba);

	editor.ZoomChanged.connect (sigc::mem_fun (*this, &TempoCurve::reposition));

	/* events will be handled by both the group and the mark itself, so
	 * make sure they can both be used to lookup this object.
	 */

	_curve->set_data ("tempo curve", this);
	_background->set_data ("tempo curve", this);

	if (handle_events) {
		//group->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_marker_event), group, this));
	}

	set_position (_tempo.frame(), UINT32_MAX);
	_curve->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_curve_event), _curve, this));
	_background->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_tempo_curve_event), _background, this));

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
	points->clear();
	unit_position = editor.sample_to_pixel (frame);
	group->set_x_position (unit_position);
	frame_position = frame;
	_end_frame = end_frame;

	const double tempo_delta = max (10.0, _max_tempo - _min_tempo);
	double max_y = 0.0;
	points = new ArdourCanvas::Points ();

	if (end_frame == UINT32_MAX) {
		_curve->set_fill_mode (ArdourCanvas::Curve::None);
		const double tempo_at = _tempo.tempo_at_frame (frame, editor.session()->frame_rate()) * _tempo.note_type();
		const double y2_pos =  (curve_height + 2.0) - (((tempo_at - _min_tempo) / (tempo_delta)) * curve_height);
		max_y = y2_pos;
		points->push_back (ArdourCanvas::Duple (0.0, y2_pos));
		points->push_back (ArdourCanvas::Duple (ArdourCanvas::COORD_MAX - 5.0, y2_pos));

	} else {
		_curve->set_fill_mode (ArdourCanvas::Curve::Inside);
		const framepos_t frame_step = (end_frame - frame) / 32;
		framepos_t current_frame = frame;
		while (current_frame < end_frame) {
			const double tempo_at = _tempo.tempo_at_frame (current_frame, editor.session()->frame_rate()) * _tempo.note_type();
			const double y2_pos = (curve_height + 2.0) - (((tempo_at - _min_tempo) / (tempo_delta)) * curve_height);

			points->push_back (ArdourCanvas::Duple (editor.sample_to_pixel (current_frame - frame), y2_pos));
			max_y = max (y2_pos, max_y);
			current_frame += frame_step;
		}
	}

	/* the background fills the gap between the bottom of the curve and the time bar */
	_background->set_x0 (0.0);
	_background->set_x1 (editor.sample_to_pixel (end_frame - frame));
	_background->set_y0 (max_y + 1.0);
	_background->set_y1 (curve_height + 2.0);

	if (max_y == curve_height + 2.0) {
		_background->hide();
	} else {
		_background->show();
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
	_curve->set_fill_color (UIConfiguration::instance().color_mod ("selection rect", "selection rect"));
	_curve->set_outline_color (_color);

	_background->set_fill (true);
	_background->set_fill_color (UIConfiguration::instance().color_mod ("selection rect", "selection rect"));
}
