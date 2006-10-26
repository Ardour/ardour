#include <ardour/curve.h>
#include <ardour/audioregion.h>
#include <pbd/memento_command.h>

#include "region_gain_line.h"
#include "audio_region_view.h"
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
	if (!rv.audio_region()->envelope_active()) {
                trackview.session().add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), &rv.audio_region()->get_state(), 0));
                rv.audio_region()->set_envelope_active(false);
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

	if (!rv.audio_region()->envelope_active()) {
                XMLNode &before = rv.audio_region()->get_state();
		rv.audio_region()->set_envelope_active(true);
                XMLNode &after = rv.audio_region()->get_state();
                trackview.session().add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), &before, &after));
	}

	alist.erase (mr.start, mr.end);

	trackview.editor.current_session()->add_command (new MementoCommand<AudioRegionGainLine>(*this, &before, &get_state()));
	trackview.editor.current_session()->commit_reversible_command ();
	trackview.editor.current_session()->set_dirty ();
}

void
AudioRegionGainLine::end_drag (ControlPoint* cp) 
{
	if (!rv.audio_region()->envelope_active()) {
		rv.audio_region()->set_envelope_active(true);
                trackview.session().add_command(new MementoCommand<AudioRegion>(*(rv.audio_region().get()), 0, &rv.audio_region()->get_state()));
	}
	AutomationLine::end_drag(cp);
}


