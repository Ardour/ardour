#include <ardour/curve.h>
#include <ardour/audioregion.h>
#include <pbd/memento_command.h>

#include "region_gain_line.h"
#include "regionview.h"
#include "utils.h"

#include "time_axis_view.h"
#include "editor.h"

#include <ardour/session.h>


#include "i18n.h"


using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioRegionGainLine::AudioRegionGainLine (const string & name, Session& s, AudioRegionView& r, ArdourCanvas::Group& parent, Curve& c)
  : AutomationLine (name, r.get_time_axis_view(), parent, c),
	  session (s),
	  rv (r)
{
	group->raise_to_top ();
	set_verbose_cursor_uses_gain_mapping (true);
	terminal_points_can_slide = false;
}

void
AudioRegionGainLine::view_to_model_y (double& y)
{
	y = slider_position_to_gain (y);
	y = max (0.0, y);
	y = min (2.0, y);
}

void
AudioRegionGainLine::model_to_view_y (double& y)
{
	y = gain_to_slider_position (y);
}

void
AudioRegionGainLine::start_drag (ControlPoint* cp, float fraction) 
{
	AutomationLine::start_drag(cp,fraction);
	if (!rv.region.envelope_active()) {
                trackview.session().add_command(new MementoUndoCommand<AudioRegion>(rv.region, rv.region.get_state()));
                rv.region.set_envelope_active(false);
	}
}

// This is an extended copy from AutomationList
void
AudioRegionGainLine::remove_point (ControlPoint& cp)
{
	ModelRepresentation mr;

	model_representation (cp, mr);

	trackview.editor.current_session()->begin_reversible_command (_("remove control point"));
        XMLNode &before = get_state();

	if (!rv.region.envelope_active()) {
                XMLNode &before = rv.region.get_state();
		rv.region.set_envelope_active(true);
                XMLNode &after = rv.region.get_state();
                trackview.session().add_command(new MementoCommand<AudioRegion>(rv.region, before, after));
	}

	alist.erase (mr.start, mr.end);

	trackview.editor.current_session()->add_command (new MementoCommand<AudioRegionGainLine>(*this, before, get_state()));
	trackview.editor.current_session()->commit_reversible_command ();
	trackview.editor.current_session()->set_dirty ();
}

void
AudioRegionGainLine::end_drag (ControlPoint* cp) 
{
	if (!rv.region.envelope_active()) {
		rv.region.set_envelope_active(true);
                trackview.session().add_command(new MementoRedoCommand<AudioRegion>(rv.region, rv.region.get_state()));
	}
	AutomationLine::end_drag(cp);
}


// This is a copy from AutomationList
UndoAction
AudioRegionGainLine::get_memento ()
{
	return alist.get_memento();
}
