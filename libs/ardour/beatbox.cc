#include "ardour/beatbox.h"

using namespace ARDOUR;
using std::string;

Beatbox::Beatbox (Session& s, string const & n)
	: Processor (s, n)
{
	_input.reset (new AsyncMIDIPort (n, PortFlags (IsInput|IsTerminal)));
}

bool
Beatbox::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	if (in.n_midi() == 0) {
		return false;
	}

	return true;
}

void
Beatbox::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, double speed, pframes_t /*nframes*/, bool /*result_required*/)
{
}

void
Beatbox::set_instrument (boost::shared_ptr<PluginInsert> p)
{
	_instrument = p;
}
