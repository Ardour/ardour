#pragma once

namespace ARDOUR {
	class Region;
}

class SlipDraggable
{
  public:
	SlipDraggable() {}
	virtual ~SlipDraggable () {}

	virtual void drag_start() = 0;
	virtual void drag_end() = 0;
	virtual std::shared_ptr<ARDOUR::Region> region() const = 0;
};

