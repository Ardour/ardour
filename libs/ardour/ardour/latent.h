#ifndef __ardour_latent_h__
#define __ardour_latent_h__

#include "ardour/types.h"

namespace ARDOUR {

class Latent {
  public:
	Latent() : _own_latency (0), _user_latency (0) {}
	virtual ~Latent() {}

	virtual framecnt_t signal_latency() const = 0;
	framecnt_t user_latency () const { return _user_latency; }

	framecnt_t effective_latency() const {
		if (_user_latency) {
			return _user_latency;
		} else {
			return signal_latency ();
		}
	}

	virtual void set_latency_delay (framecnt_t val) { _own_latency = val; }
	virtual void set_user_latency (framecnt_t val) { _user_latency = val; }

  protected:
	framecnt_t           _own_latency;
	framecnt_t           _user_latency;
};

}

#endif /* __ardour_latent_h__*/
