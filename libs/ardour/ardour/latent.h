#ifndef __ardour_latent_h__
#define __ardour_latent_h__

#include <ardour/types.h>

namespace ARDOUR {

class Latent {
  public:
	Latent() : _own_latency (0), _user_latency (0) {}
	virtual ~Latent() {}

	virtual nframes_t signal_latency() const = 0;
	nframes_t user_latency () const { return _user_latency; }

	virtual void set_latency_delay (nframes_t val) { _own_latency = val; }
	virtual void set_user_latency (nframes_t val) { _user_latency = val; }

  protected:
	nframes_t           _own_latency;
	nframes_t           _user_latency;
};

}

#endif /* __ardour_latent_h__*/
