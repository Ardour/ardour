#ifndef __CANVAS_GROUP_H__
#define __CANVAS_GROUP_H__

#include <list>
#include <vector>
#include "canvas/item.h"
#include "canvas/types.h"
#include "canvas/lookup_table.h"

namespace ArdourCanvas {

class Group : public Item
{
public:
	explicit Group (Group *);
	explicit Group (Group *, Duple);
	~Group ();

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	virtual void compute_bounding_box () const;
	XMLNode* get_state () const;
	void set_state (XMLNode const *);

	void add (Item *);
	void remove (Item *);
	std::list<Item*> const & items () const {
		return _items;
	}

	void raise_child_to_top (Item *);
	void raise_child (Item *, int);
	void lower_child_to_bottom (Item *);
	void child_changed ();

	void add_items_at_point (Duple, std::vector<Item const *> &) const;

        void dump (std::ostream&) const;

	static int default_items_per_cell;

protected:
	
	explicit Group (Canvas *);
	
private:
	friend class ::OptimizingLookupTableTest;
	
	Group (Group const &);
	void ensure_lut () const;
	void invalidate_lut () const;

	/* our items, from lowest to highest in the stack */
	std::list<Item*> _items;

	mutable LookupTable* _lut;
};

}

#endif
