#include "canvas-simplerect.h"
#include "ghostregion.h"
#include "automation_time_axis.h"
#include "rgb_macros.h"

using namespace Editing;

GhostRegion::GhostRegion (AutomationTimeAxisView& atv, double initial_pos)
	: trackview (atv)
{
	group = gnome_canvas_item_new (GNOME_CANVAS_GROUP(trackview.canvas_display),
				     gnome_canvas_group_get_type(),
				     "x", initial_pos,
				     "y", 0.0,
				     NULL);

	base_rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP(group),
					 gnome_canvas_simplerect_get_type(),
					 "x1", (double) 0.0,
					 "y1", (double) 0.0,
					 "y2", (double) trackview.height,
					 "outline_what", (guint32) 0,
					 "outline_color_rgba", color_map[cGhostTrackBaseOutline],
					 "fill_color_rgba", color_map[cGhostTrackBaseFill],
					 NULL);

	gnome_canvas_item_lower_to_bottom (group);

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
	gnome_canvas_item_set (base_rect, "x2", units, NULL);
}

void
GhostRegion::set_height ()
{
	gdouble ht;
	vector<GnomeCanvasItem*>::iterator i;
	uint32_t n;

	gnome_canvas_item_set (base_rect, "y2", (double) trackview.height, NULL);

	ht = ((trackview.height) / (double) waves.size());
	
	for (n = 0, i = waves.begin(); i != waves.end(); ++i, ++n) {
		gdouble yoff = n * ht;
		gnome_canvas_item_set ((*i), "height", ht, "y", yoff, NULL);
	}
}

