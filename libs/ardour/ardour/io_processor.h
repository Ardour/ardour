/*
    Copyright (C) 2001 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_redirect_h__
#define __ardour_redirect_h__

#include <string>
#include <vector>
#include <set>
#include <map>
#include <boost/shared_ptr.hpp>
#include <sigc++/signal.h>

#include <glibmm/thread.h>

#include <pbd/undo.h>

#include <ardour/ardour.h>
#include <ardour/processor.h>
#include <ardour/io.h>
#include <ardour/automation_event.h>

using std::map;
using std::set;
using std::string;
using std::vector;

class XMLNode;

namespace ARDOUR {

class Session;

/** A mixer strip element (Processor) with Jack ports (IO).
 */
class IOProcessor : public Processor
{
  public:
	IOProcessor (Session&, const string& name, Placement,
		  int input_min = -1, int input_max = -1, int output_min = -1, int output_max = -1);
	IOProcessor (const IOProcessor&);
	virtual ~IOProcessor ();
	
	virtual ChanCount output_streams() const { return _io->n_outputs(); }
	virtual ChanCount input_streams () const { return _io->n_inputs(); }
	virtual ChanCount natural_output_streams() const { return _io->n_outputs(); }
	virtual ChanCount natural_input_streams () const { return _io->n_inputs(); }

	boost::shared_ptr<IO>       io()       { return _io; }
	boost::shared_ptr<const IO> io() const { return _io; }
	
	virtual void automation_snapshot (nframes_t now) { _io->automation_snapshot(now); }

	virtual void run_in_place (BufferSet& in, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset) = 0;

	void silence (nframes_t nframes, nframes_t offset);

	sigc::signal<void,IOProcessor*,bool>     AutomationPlaybackChanged;
	sigc::signal<void,IOProcessor*,uint32_t> AutomationChanged;
	
	XMLNode& state (bool full_state);
	int set_state (const XMLNode&);
	
  protected:
	boost::shared_ptr<IO> _io;
};

} // namespace ARDOUR

#endif /* __ardour_redirect_h__ */
