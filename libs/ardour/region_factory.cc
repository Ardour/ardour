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
#include "pbd/boost_debug.h"

#include "ardour/session.h"

#include "ardour/region_factory.h"
#include "ardour/region.h"
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/midi_source.h"
#include "ardour/midi_region.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

PBD::Signal1<void,boost::shared_ptr<Region> > RegionFactory::CheckNewRegion;
Glib::StaticMutex RegionFactory::region_map_lock;
RegionFactory::RegionMap RegionFactory::region_map;

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<const Region> region)
{
	boost::shared_ptr<Region> ret;
	boost::shared_ptr<const AudioRegion> ar;
	boost::shared_ptr<const MidiRegion> mr;

	if ((ar = boost::dynamic_pointer_cast<const AudioRegion>(region)) != 0) {

		AudioRegion* arn = new AudioRegion (ar, 0, true);
		boost_debug_shared_ptr_mark_interesting (arn, "Region");

		boost::shared_ptr<AudioRegion> arp (arn);
		ret = boost::static_pointer_cast<Region> (arp);

	} else if ((mr = boost::dynamic_pointer_cast<const MidiRegion>(region)) != 0) {

		MidiRegion* mrn = new MidiRegion (mr, 0, true);
		boost::shared_ptr<MidiRegion> mrp (mrn);
		ret = boost::static_pointer_cast<Region> (mrp);

	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
	}

	if (ret) {
		map_add (ret);

		/* pure copy constructor - no property list */
		/* pure copy constructor - no CheckNewRegion emitted */
	}

	return ret;
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Region> region, frameoffset_t offset, const PropertyList& plist, bool announce)
{
	boost::shared_ptr<Region> ret;
	boost::shared_ptr<const AudioRegion> other_a;
	boost::shared_ptr<const MidiRegion> other_m;

	if ((other_a = boost::dynamic_pointer_cast<AudioRegion>(region)) != 0) {

		AudioRegion* ar = new AudioRegion (other_a, offset, true);
		boost_debug_shared_ptr_mark_interesting (ar, "Region");

		boost::shared_ptr<AudioRegion> arp (ar);
		ret = boost::static_pointer_cast<Region> (arp);

	} else if ((other_m = boost::dynamic_pointer_cast<MidiRegion>(region)) != 0) {

		MidiRegion* mr = new MidiRegion (other_m, offset, true);
		boost::shared_ptr<MidiRegion> mrp (mr);
		ret = boost::static_pointer_cast<Region> (mrp);

	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
		return boost::shared_ptr<Region>();
	}

	if (ret) {
		ret->set_properties (plist);
		map_add (ret);

		if (announce) {
			CheckNewRegion (ret);
		}
	}

	return ret;
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Region> region, const PropertyList& plist, bool announce)
{
	boost::shared_ptr<Region> ret;
	boost::shared_ptr<const AudioRegion> other_a;
	boost::shared_ptr<const MidiRegion> other_m;

	if ((other_a = boost::dynamic_pointer_cast<AudioRegion>(region)) != 0) {

		AudioRegion* ar = new AudioRegion (other_a, 0, false);
		boost_debug_shared_ptr_mark_interesting (ar, "Region");

		boost::shared_ptr<AudioRegion> arp (ar);
		ret = boost::static_pointer_cast<Region> (arp);

	} else if ((other_m = boost::dynamic_pointer_cast<MidiRegion>(region)) != 0) {

		MidiRegion* mr = new MidiRegion (other_m, 0, false);
		boost::shared_ptr<MidiRegion> mrp (mr);
		ret = boost::static_pointer_cast<Region> (mrp);

	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
		return boost::shared_ptr<Region>();
	}

	if (ret) {
		ret->set_properties (plist);
		map_add (ret);

		if (announce) {
			CheckNewRegion (ret);
		}
	}

	return ret;
}




boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Region> region, const SourceList& srcs, const PropertyList& plist, bool announce)
{
	boost::shared_ptr<Region> ret;
	boost::shared_ptr<const AudioRegion> other;

	/* used by AudioFilter when constructing a new region that is intended to have nearly
	   identical settings to an original, but using different sources.
	*/

	if ((other = boost::dynamic_pointer_cast<AudioRegion>(region)) != 0) {

		// XXX use me in caller where plist is setup, this is start i think srcs.front()->length (srcs.front()->timeline_position())
		
		AudioRegion* ar = new AudioRegion (other, srcs);
		boost_debug_shared_ptr_mark_interesting (ar, "Region");

		boost::shared_ptr<AudioRegion> arp (ar);
		ret = boost::static_pointer_cast<Region> (arp);

	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		/*NOTREACHED*/
	}

	if (ret) {

		ret->set_properties (plist);
		map_add (ret);

		if (announce) {
			CheckNewRegion (ret);
		}
	}

	return ret;

}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Source> src, const PropertyList& plist, bool announce)
{
	SourceList srcs;
	srcs.push_back (src);
	return create (srcs, plist, announce);
}

boost::shared_ptr<Region>
RegionFactory::create (const SourceList& srcs, const PropertyList& plist, bool announce)
{
	boost::shared_ptr<Region> ret; 
	boost::shared_ptr<AudioSource> as;
	boost::shared_ptr<MidiSource> ms;

	if ((as = boost::dynamic_pointer_cast<AudioSource>(srcs[0])) != 0) {

		AudioRegion* ar = new AudioRegion (srcs);
		boost_debug_shared_ptr_mark_interesting (ar, "Region");

		boost::shared_ptr<AudioRegion> arp (ar);
		ret = boost::static_pointer_cast<Region> (arp);

	} else if ((ms = boost::dynamic_pointer_cast<MidiSource>(srcs[0])) != 0) {
		MidiRegion* mr = new MidiRegion (srcs);
		boost_debug_shared_ptr_mark_interesting (mr, "Region");

		boost::shared_ptr<MidiRegion> mrp (mr);
		ret = boost::static_pointer_cast<Region> (mrp);
	}

	if (ret) {

		ret->set_properties (plist);
		map_add (ret);

		if (announce) {
			CheckNewRegion (ret);
		}
	}

	return ret;
}

boost::shared_ptr<Region>
RegionFactory::create (Session& session, XMLNode& node, bool yn)
{
	return session.XMLRegionFactory (node, yn);
}

boost::shared_ptr<Region>
RegionFactory::create (SourceList& srcs, const XMLNode& node)
{
	boost::shared_ptr<Region> ret;

	if (srcs.empty()) {
		return ret;
	}

	if (srcs[0]->type() == DataType::AUDIO) {

		AudioRegion* ar = new AudioRegion (srcs);
		boost_debug_shared_ptr_mark_interesting (ar, "Region");

		boost::shared_ptr<AudioRegion> arp (ar);
		ret = boost::static_pointer_cast<Region> (arp);

	} else if (srcs[0]->type() == DataType::MIDI) {
		
		MidiRegion* mr = new MidiRegion (srcs);

		boost::shared_ptr<MidiRegion> mrp (mr);
		ret = boost::static_pointer_cast<Region> (mrp);
	}

	if (ret) {

		if (ret->set_state (node, Stateful::loading_state_version)) {
			ret.reset ();
		} else {
			map_add (ret);
			CheckNewRegion (ret);
		}
	}

	return ret;
}


void
RegionFactory::map_add (boost::shared_ptr<Region> r)
{
	pair<ID,boost::shared_ptr<Region> > p;
	p.first = r->id();
	p.second = r;

        { 
                Glib::Mutex::Lock lm (region_map_lock);
                region_map.insert (p);
                /* we pay no attention to attempts to delete regions */
        }
}

void
RegionFactory::map_remove (boost::shared_ptr<Region> r)
{
        { 
                Glib::Mutex::Lock lm (region_map_lock);
                RegionMap::iterator i = region_map.find (r->id());
                if (i != region_map.end()) {
                        region_map.erase (i);
                }
        }
}

boost::shared_ptr<Region>
RegionFactory::region_by_id (const PBD::ID& id)
{
	RegionMap::iterator i = region_map.find (id);

	if (i == region_map.end()) {
                cerr << "ID " << id << " not found in region map\n";
		return boost::shared_ptr<Region>();
	}

	return i->second;
}
	
void
RegionFactory::clear_map ()
{
	region_map.clear ();
}
