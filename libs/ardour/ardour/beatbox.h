#ifndef __ardour_beatbox_h__
#define __ardour_beatbox_h__

#include <boost/shared_ptr.hpp>

#include "ardour/async_midi_port.h"
#include "ardour/processor.h"

namespace ARDOUR {

class PluginInsert;

class Beatbox : public Processor {

  public:
	Beatbox (Session&, std::string const& name);
	virtual ~Beatbox ();

	boost::shared_ptr<PluginInsert> instrument() const { return _instrument; }
	void set_instrument (boost::shared_ptr<PluginInsert>);

	boost::shared_ptr<AsyncMIDIPort> input_port () const { return _input; }

	/* processor API */

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) = 0;
	void run (BufferSet& /*bufs*/, framepos_t /*start_frame*/, framepos_t /*end_frame*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);

  protected:

  private:
	boost::shared_ptr<PluginInsert> _instrument;
	boost::shared_ptr<AsyncMIDIPort> _input;
};


} /* namespace */


#endif /* __ardour_beatbox_h__ */
