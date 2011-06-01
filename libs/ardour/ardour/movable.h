#ifndef __libardour_movable_h__
#define __libardour_movable_h__

namespace ARDOUR {

class Movable {
  public:
	Movable() {}

	bool locked () const { return false; }
};

}

#endif /* __libardour_movable_h__ */
