#include "simplerect.h"
#include "waveview.h"
#include "ghostregion.h"
#include "automation_time_axis.h"
#include "rgb_macros.h"

using namespace Editing;
using namespace ArdourCanvas;

GhostRegion::GhostRegion (AutomationTimeAxisView& atv, double initial_pos)
	: trackview (atv)
{
  //group = gnome_canvas_item_new (GNOME_CANVAS_GROUP(trackview.canvas_display),
  //			     gnome_canvas_group_get_type(),
  //			     "x", initial_pos,
  //			     "y", 0.0,
  //			     NULL);
	group = new ArdourCanvas::Group (*trackview.canvas_display);
	group->property_x() = initial_pos;
	group->property_y() = 0.0;

	base_rect = new ArdourCanvas::SimpleRect (*group);
	base_rect->property_x1() = (double) 0.0;
	base_rect->property_y1() = (double) 0.0;
	base_rect->property_y2() = (double) trackview.height;
	base_rect->property_outline_what() = (guint32) 0;
	base_rect->property_outline_color_rgba() = color_map[cGhostTrackBaseOutline];
	base_rect->property_fill_color_rgba() = color_map[cGhostTrackBaseFill];
	group->lower_to_bottom ();

	atv.add_ghost (this);
}

GhostRegion::~GhostRegion ()
{
	GoingAway (this);
	delete base_rect;
	delete group;
}

void
GhostRegion::set_samples_per_unit (double spu)
{
	for (vector<WaveView*>::iterator i = waves.begin(); i != waves.end(); ++i) {
		(*i)->property_samples_per_unit() = spu;
	}		
}

void
GhostRegion::set_duration (double units)
{
        base_rect->property_x2() = units;
}

void
GhostRegion::set_height ()
{
	gdouble ht;
	vector<WaveView*>::iterator i;
	uint32_t n;

	base_rect->property_y2() = (double) trackview.height;
	ht = ((trackview.height) / (double) waves.size());
	
	for (n = 0, i = waves.begin(); i != waves.end(); ++i, ++n) {
		gdouble yoff = n * ht;
		(*i)->property_height() = ht;
		(*i)->property_y() = yoff;
	}
}

