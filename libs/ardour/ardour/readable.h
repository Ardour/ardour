#ifndef __ardour_readable_h__
#define __ardour_readable_h__

#include "ardour/types.h"

namespace ARDOUR {

class Readable {
  public:
	Readable () {}
	virtual ~Readable() {}

	virtual nframes_t read (Sample*, sframes_t pos, nframes_t cnt, int channel) const = 0;
	virtual sframes_t readable_length() const = 0;
	virtual uint32_t  n_channels () const = 0;
};

}

#endif /* __ardour_readable_h__ */
