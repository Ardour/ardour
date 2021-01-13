/*
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_redirect_h__
#define __ardour_redirect_h__

#include <string>
#include <boost/shared_ptr.hpp>

#include <glibmm/threads.h>

#include "pbd/undo.h"

#include "ardour/ardour.h"
#include "ardour/processor.h"

class XMLNode;

namespace ARDOUR {

class Session;
class IO;
class Route;

/** A mixer strip element (Processor) with 1 or 2 IO elements. */
class LIBARDOUR_API IOProcessor : public Processor
{
public:
	IOProcessor (Session&, bool with_input, bool with_output,
	             const std::string& proc_name, const std::string io_name="",
	             ARDOUR::DataType default_type = DataType::AUDIO, bool sendish=false);

	IOProcessor (Session&, boost::shared_ptr<IO> input, boost::shared_ptr<IO> output,
	             const std::string& proc_name, Temporal::TimeDomain, bool sendish=false);

	virtual ~IOProcessor ();

	bool set_name (const std::string& str);

	bool does_routing() const { return true; }

	virtual ChanCount natural_output_streams() const;
	virtual ChanCount natural_input_streams () const;

	boost::shared_ptr<IO>       input()        { return _input; }
	boost::shared_ptr<const IO> input() const  { return _input; }
	boost::shared_ptr<IO>       output()       { return _output; }
	boost::shared_ptr<const IO> output() const { return _output; }

	void set_input (boost::shared_ptr<IO>);
	void set_output (boost::shared_ptr<IO>);

	void silence (samplecnt_t nframes, samplepos_t start_sample);
	void disconnect ();

	virtual bool feeds (boost::shared_ptr<Route> other) const;

	PBD::Signal2<void,IOProcessor*,bool>     AutomationPlaybackChanged;
	PBD::Signal2<void,IOProcessor*,uint32_t> AutomationChanged;

	XMLNode& state ();
	int set_state (const XMLNode&, int version);

	static void prepare_for_reset (XMLNode& state, const std::string& name);

	uint32_t bit_slot() const { return _bitslot; }

protected:
	boost::shared_ptr<IO> _input;
	boost::shared_ptr<IO> _output;

	/* used by PortInsert, Send & Return*/
	std::string validate_name (std::string const& new_name, std::string const& canonical_name) const;
	uint32_t _bitslot;

private:
	/* disallow copy construction */
	IOProcessor (const IOProcessor&);

	virtual int set_state_2X (const XMLNode &, int);

	bool _own_input;
	bool _own_output;

};

} // namespace ARDOUR

#endif /* __ardour_redirect_h__ */
