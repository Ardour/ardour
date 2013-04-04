#ifndef __CANVAS_FILL_H__
#define __CANVAS_FILL_H__

#include <stdint.h>
#include "canvas/item.h"

namespace ArdourCanvas {

class Fill : virtual public Item
{
public:
	Fill (Group *);

	void add_fill_state (XMLNode *) const;
	void set_fill_state (XMLNode const *);

	Color fill_color () const {
		return _fill_color;
	}
	void set_fill_color (Color);
	bool fill () const {
		return _fill;
	}
	void set_fill (bool);
	
protected:
	void setup_fill_context (Cairo::RefPtr<Cairo::Context>) const;
	
	Color _fill_color;
	bool _fill;
};

}

#endif
