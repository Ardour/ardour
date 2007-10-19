#include <cassert>
#include "ardour/auto_bundle.h"

ARDOUR::AutoBundle::AutoBundle (bool i)
	: Bundle (i)
{

}

ARDOUR::AutoBundle::AutoBundle (std::string const & n, bool i)
	: Bundle (n, i)
{
	
}

uint32_t
ARDOUR::AutoBundle::nchannels () const
{
	Glib::Mutex::Lock lm (_ports_mutex);
	return _ports.size ();
}

const ARDOUR::PortList&
ARDOUR::AutoBundle::channel_ports (uint32_t c) const
{
	assert (c < nchannels());

	Glib::Mutex::Lock lm (_ports_mutex);
	return _ports[c];
}

void
ARDOUR::AutoBundle::set_channels (uint32_t n)
{
	Glib::Mutex::Lock lm (_ports_mutex);
	_ports.resize (n);
}

void
ARDOUR::AutoBundle::set_port (uint32_t c, std::string const & p)
{
	assert (c < nchannels ());

	Glib::Mutex::Lock lm (_ports_mutex);
	_ports[c].resize (1);
	_ports[c][0] = p;
}
