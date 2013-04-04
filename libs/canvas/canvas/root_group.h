#ifndef __CANVAS_ROOT_GROUP_H__
#define __CANVAS_ROOT_GROUP_H__

#include "group.h"

namespace ArdourCanvas {

class RootGroup : public Group
{
private:
	friend class Canvas;
	
	RootGroup (Canvas *);

	void compute_bounding_box () const;
	void child_changed ();
};

}

#endif
