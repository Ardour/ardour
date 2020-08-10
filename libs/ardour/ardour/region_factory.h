/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2018-2019 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_region_factory_h__
#define __ardour_region_factory_h__

#include <glibmm/threads.h>
#include <map>
#include <set>

#include "pbd/id.h"
#include "pbd/property_list.h"
#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

class XMLNode;
class RegionNamingTest;

namespace ARDOUR {

class AudioRegion;
class Session;
class ThawList;

class LIBARDOUR_API RegionFactory
{
public:
	typedef std::map<PBD::ID, boost::shared_ptr<Region> > RegionMap;

	static boost::shared_ptr<Region> wholefile_region_by_name (const std::string& name);
	static boost::shared_ptr<Region> region_by_id (const PBD::ID&);
	static boost::shared_ptr<Region> region_by_name (const std::string& name);
	static void                      clear_map ();
	static const RegionMap           all_regions ()
	{
		return region_map;
	}

	/** This is emitted only when a new id is assigned. Therefore,
	 * in a pure Region copy, it will not be emitted.
	 *
	 * It must be emitted using a derived instance of Region, not Region
	 * itself, to permit dynamic_cast<> to be used to
	 * infer the type of Region.
	 */
	static PBD::Signal1<void, boost::shared_ptr<Region> > CheckNewRegion;

	/** create a "pure copy" of Region \p other */
	static boost::shared_ptr<Region> create (boost::shared_ptr<const Region> other, bool announce, bool fork = false, ThawList* tl = 0);

	/** Lua binding to create a "pure copy" of Region \p other */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Region> other, bool announce, bool fork)
	{
		return create (boost::shared_ptr<const Region> (other), announce, fork, 0);
	}

	/** create a region from a single Source */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Source>, const PBD::PropertyList&, bool announce = true, ThawList* tl = 0);
	/** create a region from a multiple sources */
	static boost::shared_ptr<Region> create (const SourceList&, const PBD::PropertyList&, bool announce = true, ThawList* tl = 0);
	/** create a copy of \p other starting at zero within \p other's sources */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Region> other, const PBD::PropertyList&, bool announce = true, ThawList* tl = 0);
	/** create a copy of \p other starting at \p offset within \p other */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Region> other, timecnt_t const & offset, const PBD::PropertyList&, bool announce = true, ThawList* tl = 0);
	/** create a "copy" of \p other but using a different set of sources \p srcs */
	static boost::shared_ptr<Region> create (boost::shared_ptr<Region> other, const SourceList& srcs, const PBD::PropertyList&, bool announce = true, ThawList* tl = 0);

	/** create a region with no sources, using XML state */
	static boost::shared_ptr<Region> create (Session&, XMLNode&, bool);
	/** create a region with specified sources \p srcs and XML state */
	static boost::shared_ptr<Region> create (SourceList& srcs, const XMLNode&);

	static boost::shared_ptr<Region> get_whole_region_for_source (boost::shared_ptr<ARDOUR::Source>);

	static void get_regions_using_source (boost::shared_ptr<Source>, std::set<boost::shared_ptr<Region> >&);
	static void remove_regions_using_source (boost::shared_ptr<Source>);

	static void map_remove (boost::weak_ptr<Region>);
	static void delete_all_regions ();

	static const RegionMap& regions ()
	{
		return region_map;
	}

	static uint32_t nregions ();

	static void foreach_region (boost::function<void (boost::shared_ptr<Region>)> f)
	{
		Glib::Threads::Mutex::Lock ls (region_map_lock);
		for (RegionMap::const_iterator i = region_map.begin (); i != region_map.end (); ++i) {
			f ((*i).second);
		}
	}

	static int         region_name (std::string&, std::string, bool new_level = false);
	static std::string new_region_name (std::string);
	static std::string compound_region_name (const std::string& playlist, uint32_t compound_ops, uint32_t depth, bool whole_source);

	/* when we make a compound region, for every region involved there
	 * are two "instances" - the original, which is removed from this
	 * playlist, and a copy, which is added to the playlist used as
	 * the source for the compound.
	 *
	 * when we uncombine, we want to put the originals back into this
	 * playlist after we remove the compound. this map lets us
	 * look them up easily. note that if the compound was trimmed or
	 * split, we may have to trim the originals
	 * and they may not be added back if the compound was trimmed
	 * or split sufficiently.
	 */

	typedef std::map<boost::shared_ptr<Region>, boost::shared_ptr<Region> > CompoundAssociations;

	static CompoundAssociations& compound_associations ()
	{
		return _compound_associations;
	}

	static void add_compound_association (boost::shared_ptr<Region>, boost::shared_ptr<Region>);

	/* exposed because there may be cases where regions are created with
	 * announce=false but they still need to be in the map soon after
	 * creation.
	 */

	static void map_add (boost::shared_ptr<Region>);

private:
	friend class ::RegionNamingTest;

	static void region_changed (PBD::PropertyChange const&, boost::weak_ptr<Region>);
	static void add_to_region_name_maps (boost::shared_ptr<Region>);
	static void rename_in_region_name_maps (boost::shared_ptr<Region>);
	static void update_region_name_number_map (boost::shared_ptr<Region>);
	static void remove_from_region_name_map (std::string);

	static Glib::Threads::Mutex region_map_lock;
	static RegionMap            region_map;

	static Glib::Threads::Mutex region_name_maps_mutex;
	/** map of partial region names and suffix numbers */
	static std::map<std::string, uint32_t> region_name_number_map;
	/** map of complete region names with their region ID */
	static std::map<std::string, PBD::ID> region_name_map;

	static PBD::ScopedConnectionList* region_list_connections;
	static CompoundAssociations       _compound_associations;
};

} // namespace ARDOUR

#endif /* __ardour_region_factory_h__  */
