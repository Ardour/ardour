#include "canvas-simplerect.h"
#include "ghostregion.h"
#include "automation_time_axis.h"
#include "rgb_macros.h"

using namespace Editing;

GhostRegion::GhostRegion (AutomationTimeAxisView& atv, double initial_pos)
	: trackview (atv)
{
  //group = gnome_canvas_item_new (GNOME_CANVAS_GROUP(trackview.canvas_display),
  //			     gnome_canvas_group_get_type(),
  //			     "x", initial_pos,
  //			     "y", 0.0,
  //			     NULL);
	group = new Gnome::Canvas::Group (*trackview.canvas_display);
	group->set_property ("x", initial_pos);
	group->set_property ("y", 0.0);

	base_rect = new Gnome::Canvas::SimpleRect (*group);
	base_rect->set_property ("x1", (double) 0.0);
	base_rect->set_property ("y1", (double) 0.0);
	base_rect->set_property ("y2", (double) trackview.height);
	base_rect->set_property ("outline_what", (guint32) 0);
	base_rect->set_property ("outline_color_rgba", color_map[cGhostTrackBaseOutline]);
	base_rect->set_property ("fill_color_rgba", color_map[cGhostTrackBaseFill]);
	group->lower_to_bottom ();

	atv.add_ghost (this);
}

GhostRegion::~GhostRegion ()
{
	GoingAway (this);
	gtk_object_destroy (GTK_OBJECT(group));
}

void
GhostRegion::set_samples_per_unit (double spu)
{
	for (vector<GnomeCanvasItem*>::iterator i = waves.begin(); i != waves.end(); ++i) {
	        gnome_canvas_item_set ((*i), "samples_per_unit", spu, NULL);
	}		
}

void
GhostRegion::set_duration (double units)
{
        base_rect->set_property ("x2", units);
}

void
GhostRegion::set_height ()
{
	gdouble ht;
	vector<GnomeCanvasItem*>::iterator i;
	uint32_t n;

	base_rect->set_property ("y2", (double) trackview.height);
	ht = ((trackview.height) / (double) waves.size());
	
	for (n = 0, i = waves.begin(); i != waves.end(); ++i, ++n) {
		gdouble yoff = n * ht;
		gnome_canvas_item_set ((*i), "height", ht, "y", yoff, NULL);
	}
}

