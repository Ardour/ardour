/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_graph_h__
#define __ardour_graph_h__

#include <boost/shared_ptr.hpp>

#include "ardour/types.h"

namespace ARDOUR
{

/// Takes data in
template <typename T>
class GraphSink  {
  public:
	GraphSink () : end_of_input (false) {}
	virtual ~GraphSink () { end_of_input = false; }
	
	// writes data and return number of frames written
	virtual nframes_t write (T * data, nframes_t frames) = 0;
	
	// Notifies end of input. All left over data must be written at this stage
	virtual void set_end_of_input (bool state = true)
	{
		end_of_input = state;
	}

  protected:
	bool end_of_input;
};

/// is a source for data
template <typename T>
class GraphSource  {
  public:
	GraphSource () {}
	virtual ~GraphSource () {}
	
	virtual nframes_t read (T * data, nframes_t frames) = 0;
};

/// Takes data in, processes it and passes it on to another sink
template <typename TIn, typename TOut>
class GraphSinkVertex : public GraphSink<TIn> {
  public:
	GraphSinkVertex () {}
	virtual ~GraphSinkVertex () {}
	
	void pipe_to (boost::shared_ptr<GraphSink<TOut> > dest) {
		piped_to = dest;
	}
	
	nframes_t write (TIn * data, nframes_t frames)
	{
		if (!piped_to) {
			return -1;
		}
		return process (data, frames);
	}	
	
	virtual void set_end_of_input (bool state = true)
	{
		if (!piped_to) {
			return;
		}
		piped_to->set_end_of_input (state);
		GraphSink<TIn>::end_of_input = state;
	}
	
  protected:
	boost::shared_ptr<GraphSink<TOut> > piped_to;
	
	/* process must process data,
	   use piped_to->write to write the data
	   and return number of frames written */
	virtual nframes_t process (TIn * data, nframes_t frames) = 0;
};

} // namespace ARDOUR

#endif /* __ardour_graph_h__ */

