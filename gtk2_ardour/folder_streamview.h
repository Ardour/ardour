#pragma once

#include <memory>

#include "canvas/container.h"
#include "temporal/timeline.h"

namespace ARDOUR {
	class Folder;
	class Track;
}

class EditingContext;

class FolderStreamView : public ArdourCanvas::Container
{
   public:
	FolderStreamView (EditingContext& ec, ArdourCanvas::Item* parent, std::shared_ptr<ARDOUR::Folder> folder);
	std::shared_ptr<ARDOUR::Folder> folder() { return _folder; }

	void set_height (int);

	void render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
	void compute_bounding_box () const;

   private:
	EditingContext& editing_context;
	std::shared_ptr<ARDOUR::Folder> _folder;
	typedef std::pair<Temporal::timepos_t,Temporal::timepos_t> RegionDuration;
	typedef std::list<RegionDuration> PositionList;
	std::map<std::shared_ptr<ARDOUR::Track>,PositionList> region_map;
	double _height;
	double max_x_extent;

	void refill_region_map ();
};
