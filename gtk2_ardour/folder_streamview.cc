#include "ardour/folder.h"
#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/track.h"

#include "gtkmm2ext/colors.h"

#include "editing_context.h"
#include "gui_thread.h"
#include "folder_streamview.h"

using namespace ArdourCanvas;
using namespace ARDOUR;
using namespace Gtkmm2ext;

FolderStreamView::FolderStreamView (EditingContext& ec, Item* parent, std::shared_ptr<Folder> f)
	: Container (parent)
	, editing_context (ec)
	, _folder (f)
	, _height (0.)
	, max_x_extent (0.)
{
	refill_region_map ();
}

void
FolderStreamView::set_height (int h)
{
	_height = h;
	redraw ();
}

void
FolderStreamView::change_happened ()
{
	refill_region_map ();
	redraw ();
}

void
FolderStreamView::refill_region_map ()
{
	std::vector<std::shared_ptr<Track>> tracks;
	Temporal::timepos_t max_time;

	_folder->get_content_tracks (tracks);

	region_map.clear ();
	listening.drop_connections ();

	for (auto const & t : tracks) {

		std::shared_ptr<Playlist> pl (t->playlist());

		t->PlaylistChanged.connect (listening, invalidator(*this), std::bind (&FolderStreamView::change_happened, this), gui_context());
		pl->ContentsChanged.connect (listening, invalidator(*this), std::bind (&FolderStreamView::change_happened, this), gui_context());

		PositionList positions;

		std::shared_ptr<RegionList> rl (pl->region_list());

		for (auto const & r : *rl) {
			RegionDuration rd (r->position(), r->end_position());
			max_time = std::max (max_time, r->end_position());
			positions.push_back (rd);
		}

		region_map.insert (std::make_pair (t, positions));
	}

	max_x_extent = max_time.samples() / editing_context.get_current_zoom();
}

void
FolderStreamView::compute_bounding_box () const
{
	_bounding_box = Rect (0, 0, max_x_extent, _height);
	set_bbox_clean ();
}

void
FolderStreamView::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Container::render (area, context);

	uint32_t ntracks = region_map.size();

	if (_height < ntracks) {
		/* don't be ridiculous */
		return;
	}

	uint32_t height_per_track = _height / ntracks;
	double ypos = 0;
	double spp (editing_context.get_current_zoom());

	for (auto & [track,positions] : region_map) {

		Gtkmm2ext::Color fill_color (track->presentation_info().color ());
		Gtkmm2ext::Color stroke_color (Gtkmm2ext::HSV (fill_color).darker(1.1).color());

		for (auto & pos : positions) {

			Duple p0 (pos.first.samples() / spp, ypos + 1);
			Duple p1 (pos.second.samples() / spp, ypos + height_per_track - 2);

			p0 = item_to_window (p0);
			p1 = item_to_window (p1);

			/* draw rectangle with p0, p1 */
			context->rectangle (p0.x, p0.y, p1.x - p0.x, p1.y - p0.y);
			set_source_rgba (context, fill_color);
			context->fill_preserve ();
			set_source_rgba (context, stroke_color);
			context->stroke ();
		}

		ypos += height_per_track;
	}
}
