#pragma once

#include <memory>

#include "canvas/container.h"

namespace ARDOUR {
	class Folder;
}

class FolderStreamView : public ArdourCanvas::Container
{
   public:
	FolderStreamView (ArdourCanvas::Item* parent, std::shared_ptr<ARDOUR::Folder> folder);
	std::shared_ptr<ARDOUR::Folder> folder() { return _folder; }

   private:
	std::shared_ptr<ARDOUR::Folder> _folder;
	std::list<ArdourCanvas::Rect> _rectangles;
};
