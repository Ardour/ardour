#ifndef __CANVAS_OUTLINE_H__
#define __CANVAS_OUTLINE_H__

#include <stdint.h>
#include "canvas/types.h"
#include "canvas/item.h"

namespace ArdourCanvas {

class Outline : virtual public Item
{
public:
	Outline (Group *);
	virtual ~Outline () {}

	void add_outline_state (XMLNode *) const;
	void set_outline_state (XMLNode const *);
	
	Color outline_color () const {
		return _outline_color;
	}

	virtual void set_outline_color (Color);

	Distance outline_width () const {
		return _outline_width;
	}
	
	virtual void set_outline_width (Distance);

	bool outline () const {
		return _outline;
	}

	void set_outline (bool);

#ifdef CANVAS_COMPATIBILITY
	int& property_first_arrowhead () {
		return _foo_int;
	}
	int& property_last_arrowhead () {
		return _foo_int;
	}
	int& property_arrow_shape_a () {
		return _foo_int;
	}
	int& property_arrow_shape_b () {
		return _foo_int;
	}
	int& property_arrow_shape_c () {
		return _foo_int;
	}
	bool& property_draw () {
		return _foo_bool;
	}
#endif	

protected:

	void setup_outline_context (Cairo::RefPtr<Cairo::Context>) const;
	
	Color _outline_color;
	Distance _outline_width;
	bool _outline;

#ifdef CANVAS_COMPATIBILITY
	int _foo_int;
	bool _foo_bool;
#endif
};

}

#endif
