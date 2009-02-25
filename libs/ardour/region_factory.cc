/*
    Copyright (C) 2000-2006 Paul Davis 

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

#include "pbd/error.h"

#include "ardour/session.h"

#include "ardour/region_factory.h"
#include "ardour/region.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/midi_source.h"
#include "ardour/midi_region.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

sigc::signal<void,boost::shared_ptr<Region> > RegionFactory::CheckNewRegion;

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Region> region, nframes_t start, 
		       nframes_t length, const std::string& name, 
		       layer_t layer, Region::Flag flags, bool announce)
{
	boost::shared_ptr<const AudioRegion> other_a;
	boost::shared_ptr<const MidiRegion> other_m;

	if ((other_a = boost::dynamic_pointer_cast<AudioRegion>(region)) != 0) {
		AudioRegion* ar = new AudioRegion (other_a, start, length, name, layer, flags);
		boost::shared_ptr<AudioRegion> arp (ar);
		boost::shared_ptr<Region> ret (boost::static_pointer_cast<Region> (arp));
		if (announce) {
			CheckNewRegion (ret);
		}
		return ret;
	} else if ((other_m = boost::dynamic_pointer_cast<MidiRegion>(region)) != 0) {
		MidiRegion* ar = new MidiRegion (other_m, start, length, name, layer, flags);
		boost::shared_ptr<MidiRegion> arp (ar);
		boost::shared_ptr<Region> ret (boost::static_pointer_cast<Region> (arp));
		if (announce) {
			CheckNewRegion (ret);
		}
		return ret;
	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
		return boost::shared_ptr<Region>();
	}
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<const Region> region)
{
	boost::shared_ptr<const AudioRegion> ar;
	boost::shared_ptr<const MidiRegion> mr;

	if ((ar = boost::dynamic_pointer_cast<const AudioRegion>(region)) != 0) {
		boost::shared_ptr<Region> ret (new AudioRegion (ar));
		/* pure copy constructor - no CheckNewRegion emitted */
		return ret;
	} else if ((mr = boost::dynamic_pointer_cast<const MidiRegion>(region)) != 0) {
		boost::shared_ptr<Region> ret (new MidiRegion (mr));
		/* pure copy constructor - no CheckNewRegion emitted */
		return ret;
	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
		return boost::shared_ptr<Region>();
	}
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<AudioRegion> region, nframes_t start, 
		       nframes_t length, const std::string& name, 
		       layer_t layer, Region::Flag flags, bool announce)
{
	return create (boost::static_pointer_cast<Region> (region), start, length, name, layer, flags, announce);
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Region> region, const SourceList& srcs,
		       const std::string& name, layer_t layer, Region::Flag flags, bool announce)

{
	boost::shared_ptr<const AudioRegion> other;

	/* used by AudioFilter when constructing a new region that is intended to have nearly
	   identical settings to an original, but using different sources.
	*/

	if ((other = boost::dynamic_pointer_cast<AudioRegion>(region)) != 0) {
		AudioRegion* ar = new AudioRegion (other, srcs, srcs.front()->length(srcs.front()->timeline_position()), name, layer, flags);
		boost::shared_ptr<AudioRegion> arp (ar);
		boost::shared_ptr<Region> ret (boost::static_pointer_cast<Region> (arp));
		if (announce) {
			CheckNewRegion (ret);
		}
		return ret;
	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
		return boost::shared_ptr<Region>();
	}
}

boost::shared_ptr<Region>
RegionFactory::create (Session& session, XMLNode& node, bool yn)
{
	boost::shared_ptr<Region> r = session.XMLRegionFactory (node, yn);

	if (r) {
		CheckNewRegion (r);
	}

	return r;
}
	
boost::shared_ptr<Region> 
RegionFactory::create (const SourceList& srcs, nframes_t start, nframes_t length, const string& name, layer_t layer, Region::Flag flags, bool announce)
{
	if (srcs.empty()) {
		return boost::shared_ptr<Region>();
	}

	if (srcs[0]->type() == DataType::AUDIO) {
		
		AudioRegion* ar = new AudioRegion (srcs, start, length, name, layer, flags);
		boost::shared_ptr<AudioRegion> arp (ar);
		boost::shared_ptr<Region> ret (boost::static_pointer_cast<Region> (arp));
		if (announce) {
			CheckNewRegion (ret);
		}
		return ret;

	} else if (srcs[0]->type() == DataType::MIDI) {
		
		MidiRegion* ar = new MidiRegion (srcs, start, length, name, layer, flags);
		boost::shared_ptr<MidiRegion> mrp (ar);
		boost::shared_ptr<Region> ret (boost::static_pointer_cast<Region> (mrp));
		if (announce) {
			CheckNewRegion (ret);
		}
		return ret;

	}

	return boost::shared_ptr<Region> ();
}	

boost::shared_ptr<Region> 
RegionFactory::create (SourceList& srcs, const XMLNode& node)
{
	if (srcs.empty()) {
		return boost::shared_ptr<Region>();
	}

	if (srcs[0]->type() == DataType::AUDIO) {
		boost::shared_ptr<Region> ret (new AudioRegion (srcs, node));
		CheckNewRegion (ret);
		return ret;
	} else if (srcs[0]->type() == DataType::MIDI) {
		boost::shared_ptr<Region> ret (new MidiRegion (srcs, node));
		CheckNewRegion (ret);
		return ret;
	}

	return boost::shared_ptr<Region> ();
}

boost::shared_ptr<Region> 
RegionFactory::create (boost::shared_ptr<Source> src, nframes_t start, nframes_t length, const string& name, layer_t layer, Region::Flag flags, bool announce)
{
	boost::shared_ptr<AudioSource> as;
	boost::shared_ptr<MidiSource> ms;

	if ((as = boost::dynamic_pointer_cast<AudioSource>(src)) != 0) {
		boost::shared_ptr<Region> ret (new AudioRegion (as, start, length, name, layer, flags));
		if (announce) {
			CheckNewRegion (ret);
		}
		return ret;
	} else if ((ms = boost::dynamic_pointer_cast<MidiSource>(src)) != 0) {
		boost::shared_ptr<Region> ret (new MidiRegion (ms, start, length, name, layer, flags));
		if (announce) {
			CheckNewRegion (ret);
		}
		return ret;
	}

	return boost::shared_ptr<Region>();
}
