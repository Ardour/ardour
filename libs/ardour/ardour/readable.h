#ifndef __ardour_readable_h__
#define __ardour_readable_h__

#include <ardour/types.h>

namespace ARDOUR {

class Readable {
  public:
	Readable () {}
	virtual ~Readable() {}

	virtual nframes64_t read (Sample*, nframes64_t pos, nframes64_t cnt, int channel) const = 0;
	virtual nframes64_t readable_length() const = 0;
	virtual uint32_t    n_channels () const = 0;
};

}

#endif /* __ardour_readable_h__ */
