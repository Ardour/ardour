/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_region_factory_h__
#define __ardour_region_factory_h__

#include <map>

#include "pbd/id.h"

#include "ardour/types.h"
#include "ardour/region.h"

class XMLNode;

namespace ARDOUR {

class Session;
class AudioRegion;

class RegionFactory {

  public:

	static boost::shared_ptr<Region> region_by_id (const PBD::ID&);
	static void clear_map ();

	/** This is emitted only when a new id is assigned. Therefore,
	   in a pure Region copy, it will not be emitted.

	   It must be emitted using a derived instance of Region, not Region
	   itself, to permit dynamic_cast<> to be used to
	   infer the type of Region.
	*/
	static PBD::Signal1<void,boost::shared_ptr<Region> >  CheckNewRegion;

	/** create a "pure copy" of Region @param other */
	static boost::shared_ptr<Region> create (boost::shared_ptr<const Region> other);

	/** create a region from a single Source */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Source>, 
						 const PBD::PropertyList&, bool announce = true);
	/** create a region from a multiple sources */
	static boost::shared_ptr<Region> create (const SourceList &, 
						 const PBD::PropertyList&, bool announce = true);
	/** create a copy of @other starting at zero within @param other's sources */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Region> other, 
						 const PBD::PropertyList&, bool announce = true);
	/** create a copy of @other starting at @param offset within @param other */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Region>, frameoffset_t offset, 
						 const PBD::PropertyList&, bool announce = true);
	/** create a "copy" of @param other but using a different set of sources @param srcs */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Region> other, const SourceList& srcs, 
						 const PBD::PropertyList&, bool announce = true);
	
	/** create a region with no sources, using XML state */
	static boost::shared_ptr<Region> create (Session&, XMLNode&, bool);
	/** create a region with specified sources @param srcs and XML state */
	static boost::shared_ptr<Region> create (SourceList& srcs, const XMLNode&);

  private:
	static std::map<PBD::ID,boost::weak_ptr<Region> > region_map;
	static void map_add (boost::shared_ptr<Region>);
};

}

#endif /* __ardour_region_factory_h__  */
