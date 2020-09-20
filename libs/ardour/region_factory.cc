/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <inttypes.h>

#include "pbd/basename.h"
#include "pbd/error.h"

#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/boost_debug.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/thawlist.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

PBD::Signal1<void, boost::shared_ptr<Region> > RegionFactory::CheckNewRegion;
Glib::Threads::Mutex                           RegionFactory::region_map_lock;
RegionFactory::RegionMap                       RegionFactory::region_map;
PBD::ScopedConnectionList*                     RegionFactory::region_list_connections = 0;
Glib::Threads::Mutex                           RegionFactory::region_name_maps_mutex;
std::map<std::string, uint32_t>                RegionFactory::region_name_number_map;
std::map<std::string, PBD::ID>                 RegionFactory::region_name_map;
RegionFactory::CompoundAssociations            RegionFactory::_compound_associations;

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<const Region> region, bool announce, bool fork, ThawList* tl)
{
	boost::shared_ptr<Region>            ret;
	boost::shared_ptr<const AudioRegion> ar;
	boost::shared_ptr<const MidiRegion>  mr;

	if ((ar = boost::dynamic_pointer_cast<const AudioRegion> (region)) != 0) {
		ret = boost::shared_ptr<Region> (new AudioRegion (ar, timecnt_t::from_superclock (0)));

	} else if ((mr = boost::dynamic_pointer_cast<const MidiRegion> (region)) != 0) {
		if (mr->session ().config.get_midi_copy_is_fork () || fork) {
			/* What we really want to do here is what Editor::fork_region()
			   does via Session::create_midi_source_by_stealing_name(), but we
			   don't have a Track.  We'll just live with the skipped number,
			   and store the ancestral name of sources so multiple clones
			   generates reasonable names that don't have too many suffixes. */
			const std::string ancestor_name = mr->sources ().front ()->ancestor_name ();
			const std::string base          = PBD::basename_nosuffix (ancestor_name);

			boost::shared_ptr<MidiSource> source = mr->session ().create_midi_source_for_session (base);
			source->set_ancestor_name (mr->sources ().front ()->name ());
			ret = mr->clone (source, tl);
		} else {
			ret = boost::shared_ptr<Region> (new MidiRegion (mr, timecnt_t (Temporal::Beats (), timepos_t (Temporal::Beats()))));
		}

	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		abort (); /*NOTREACHED*/
	}

	if (ret) {
		if (tl) {
			tl->add (ret);
		}

		ret->set_name (new_region_name (ret->name ()));

		if (ret->session().config.get_glue_new_regions_to_bars_and_beats() && ret->position_time_domain() != Temporal::BeatTime) {
			ret->set_position_time_domain (Temporal::BeatTime);
		}

		/* pure copy constructor - no property list */
		if (announce) {
			map_add (ret);
			CheckNewRegion (ret); /* EMIT SIGNAL */
		}
	}

	BOOST_MARK_REGION (ret);
	return ret;
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Region> region, const PropertyList& plist, bool announce, ThawList* tl)
{
	boost::shared_ptr<Region>            ret;
	boost::shared_ptr<const AudioRegion> other_a;
	boost::shared_ptr<const MidiRegion>  other_m;

	if ((other_a = boost::dynamic_pointer_cast<AudioRegion> (region)) != 0) {
		ret = boost::shared_ptr<Region> (new AudioRegion (other_a));

	} else if ((other_m = boost::dynamic_pointer_cast<MidiRegion> (region)) != 0) {
		ret = boost::shared_ptr<Region> (new MidiRegion (other_m));

	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		abort (); /*NOTREACHED*/
		return boost::shared_ptr<Region> ();
	}

	if (ret) {
		if (tl) {
			tl->add (ret);
		}

		ret->apply_changes (plist);

		if (ret->session().config.get_glue_new_regions_to_bars_and_beats() && ret->position_time_domain() != Temporal::BeatTime) {
			ret->set_position_time_domain (Temporal::BeatTime);
		}

		if (announce) {
			map_add (ret);
			CheckNewRegion (ret); /* EMIT SIGNAL */
		}
	}

	BOOST_MARK_REGION (ret);
	return ret;
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Region> region, timecnt_t const & offset, const PropertyList& plist, bool announce, ThawList* tl)
{
	boost::shared_ptr<Region>            ret;
	boost::shared_ptr<const AudioRegion> other_a;
	boost::shared_ptr<const MidiRegion>  other_m;

	if ((other_a = boost::dynamic_pointer_cast<AudioRegion> (region)) != 0) {
		ret = boost::shared_ptr<Region> (new AudioRegion (other_a, offset));

	} else if ((other_m = boost::dynamic_pointer_cast<MidiRegion> (region)) != 0) {
		ret = boost::shared_ptr<Region> (new MidiRegion (other_m, offset));

	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		abort (); /*NOTREACHED*/
		return boost::shared_ptr<Region> ();
	}

	if (ret) {
		if (tl) {
			tl->add (ret);
		}
		ret->apply_changes (plist);

		if (ret->session().config.get_glue_new_regions_to_bars_and_beats() && ret->position_time_domain() != Temporal::BeatTime) {
			ret->set_position_time_domain (Temporal::BeatTime);
		}

		if (announce) {
			map_add (ret);
			CheckNewRegion (ret); /* EMIT SIGNAL */
		}
	}

	BOOST_MARK_REGION (ret);
	return ret;
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Region> region, const SourceList& srcs, const PropertyList& plist, bool announce, ThawList* tl)
{
	boost::shared_ptr<Region>            ret;
	boost::shared_ptr<const AudioRegion> other;

	/* used by AudioFilter when constructing a new region that is intended to have nearly
	   identical settings to an original, but using different sources.
	*/

	if ((other = boost::dynamic_pointer_cast<AudioRegion> (region)) != 0) {
		// XXX use me in caller where plist is setup, this is start i think srcs.front()->length (srcs.front()->natural_position())

		ret = boost::shared_ptr<Region> (new AudioRegion (other, srcs));

	} else {
		fatal << _("programming error: RegionFactory::create() called with unknown Region type")
		      << endmsg;
		abort (); /*NOTREACHED*/
	}

	if (ret) {
		if (tl) {
			tl->add (ret);
		}

		ret->apply_changes (plist);

		if (ret->session().config.get_glue_new_regions_to_bars_and_beats() && ret->position_time_domain() != Temporal::BeatTime) {
			ret->set_position_time_domain (Temporal::BeatTime);
		}

		if (announce) {
			map_add (ret);
			CheckNewRegion (ret); /* EMIT SIGNAL */
		}
	}

	BOOST_MARK_REGION (ret);
	return ret;
}

boost::shared_ptr<Region>
RegionFactory::create (boost::shared_ptr<Source> src, const PropertyList& plist, bool announce, ThawList* tl)
{
	SourceList srcs;
	srcs.push_back (src);
	return create (srcs, plist, announce, tl);
}

boost::shared_ptr<Region>
RegionFactory::create (const SourceList& srcs, const PropertyList& plist, bool announce, ThawList* tl)
{
	boost::shared_ptr<Region>      ret;
	boost::shared_ptr<AudioSource> as;
	boost::shared_ptr<MidiSource>  ms;

	if ((as = boost::dynamic_pointer_cast<AudioSource> (srcs[0])) != 0) {
		ret = boost::shared_ptr<Region> (new AudioRegion (srcs));

	} else if ((ms = boost::dynamic_pointer_cast<MidiSource> (srcs[0])) != 0) {
		ret = boost::shared_ptr<Region> (new MidiRegion (srcs));
	}

	if (ret) {
		if (tl) {
			tl->add (ret);
		}

		ret->apply_changes (plist);

		if (ret->session().config.get_glue_new_regions_to_bars_and_beats() && ret->position_time_domain() != Temporal::BeatTime) {
			ret->set_position_time_domain (Temporal::BeatTime);
		}

		if (announce) {
			map_add (ret);
			CheckNewRegion (ret); /* EMIT SIGNAL */
		}
	}

	BOOST_MARK_REGION (ret);
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

	if (srcs.empty ()) {
		return ret;
	}

	if (srcs[0]->type () == DataType::AUDIO) {
		ret = boost::shared_ptr<Region> (new AudioRegion (srcs));

	} else if (srcs[0]->type () == DataType::MIDI) {
		ret = boost::shared_ptr<Region> (new MidiRegion (srcs));
	}

	if (ret) {
		if (ret->set_state (node, Stateful::loading_state_version)) {
			ret.reset ();
		} else {
			map_add (ret);

			/* Don't fiddle with position_lock_style here as the region
			   description is coming from XML.
			*/

			CheckNewRegion (ret); /* EMIT SIGNAL */
		}
	}

	BOOST_MARK_REGION (ret);
	return ret;
}

void
RegionFactory::map_add (boost::shared_ptr<Region> r)
{
	pair<ID, boost::shared_ptr<Region> > p;
	p.first  = r->id ();
	p.second = r;

	{
		Glib::Threads::Mutex::Lock lm (region_map_lock);
		region_map.insert (p);
	}

	if (!region_list_connections) {
		region_list_connections = new ScopedConnectionList;
	}

	r->DropReferences.connect_same_thread (*region_list_connections, boost::bind (&RegionFactory::map_remove, boost::weak_ptr<Region> (r)));
	r->PropertyChanged.connect_same_thread (*region_list_connections, boost::bind (&RegionFactory::region_changed, _1, boost::weak_ptr<Region> (r)));

	add_to_region_name_maps (r);
}

void
RegionFactory::map_remove (boost::weak_ptr<Region> w)
{
	boost::shared_ptr<Region> r = w.lock ();
	if (!r) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (region_map_lock);
	RegionMap::iterator        i = region_map.find (r->id ());

	if (i != region_map.end ()) {
		remove_from_region_name_map (i->second->name ());
		region_map.erase (i);
	}
}

boost::shared_ptr<Region>
RegionFactory::region_by_id (const PBD::ID& id)
{
	RegionMap::iterator i = region_map.find (id);

	if (i == region_map.end ()) {
		return boost::shared_ptr<Region> ();
	}

	return i->second;
}

boost::shared_ptr<Region>
RegionFactory::wholefile_region_by_name (const std::string& name)
{
	for (RegionMap::iterator i = region_map.begin (); i != region_map.end (); ++i) {
		if (i->second->whole_file () && i->second->name () == name) {
			return i->second;
		}
	}
	return boost::shared_ptr<Region> ();
}

boost::shared_ptr<Region>
RegionFactory::region_by_name (const std::string& name)
{
	for (RegionMap::iterator i = region_map.begin (); i != region_map.end (); ++i) {
		if (i->second->name () == name) {
			return i->second;
		}
	}
	return boost::shared_ptr<Region> ();
}

void
RegionFactory::clear_map ()
{
	if (region_list_connections) {
		region_list_connections->drop_connections ();
	}

	{
		Glib::Threads::Mutex::Lock lm (region_map_lock);
		region_map.clear ();
		_compound_associations.clear ();
		region_name_map.clear ();
	}
}

void
RegionFactory::delete_all_regions ()
{
	RegionMap copy;

	/* copy region list */
	{
		Glib::Threads::Mutex::Lock lm (region_map_lock);
		copy = region_map;
	}

	/* clear existing map */
	clear_map ();

	/* tell everyone to drop references */
	for (RegionMap::iterator i = copy.begin (); i != copy.end (); ++i) {
		i->second->drop_references ();
	}

	/* the copy should now hold the only references, which will
	   vanish as we leave this scope, thus calling all destructors.
	*/
}

uint32_t
RegionFactory::nregions ()
{
	Glib::Threads::Mutex::Lock lm (region_map_lock);
	return region_map.size ();
}

/** Add a region to the two region name maps */
void
RegionFactory::add_to_region_name_maps (boost::shared_ptr<Region> region)
{
	update_region_name_number_map (region);

	Glib::Threads::Mutex::Lock lm (region_name_maps_mutex);
	region_name_map[region->name ()] = region->id ();
}

/** Account for a region rename in the two region name maps */
void
RegionFactory::rename_in_region_name_maps (boost::shared_ptr<Region> region)
{
	update_region_name_number_map (region);

	Glib::Threads::Mutex::Lock lm (region_name_maps_mutex);

	map<string, PBD::ID>::iterator i = region_name_map.begin ();
	while (i != region_name_map.end () && i->second != region->id ()) {
		++i;
	}

	/* Erase the entry for the old name and put in a new one */
	if (i != region_name_map.end ()) {
		region_name_map.erase (i);
		region_name_map[region->name ()] = region->id ();
	}
}

/** Remove a region's details from the region_name_map */
void
RegionFactory::remove_from_region_name_map (string n)
{
	map<string, PBD::ID>::iterator i = region_name_map.find (n);
	if (i != region_name_map.end ()) {
		region_name_map.erase (i);
	}
}

/** Update a region's entry in the region_name_number_map */
void
RegionFactory::update_region_name_number_map (boost::shared_ptr<Region> region)
{
	string::size_type const last_period = region->name ().find_last_of ('.');

	if (last_period != string::npos && last_period < region->name ().length () - 1) {
		string const base   = region->name ().substr (0, last_period);
		string const number = region->name ().substr (last_period + 1);

		/* note that if there is no number, we get zero from atoi,
		   which is just fine
		*/

		Glib::Threads::Mutex::Lock lm (region_name_maps_mutex);
		region_name_number_map[base] = atoi (number.c_str ());
	}
}

void
RegionFactory::region_changed (PropertyChange const& what_changed, boost::weak_ptr<Region> w)
{
	boost::shared_ptr<Region> r = w.lock ();
	if (!r) {
		return;
	}

	if (what_changed.contains (Properties::name)) {
		rename_in_region_name_maps (r);
	}
}

int
RegionFactory::region_name (string& result, string base, bool newlevel)
{
	char   buf[16];
	string subbase;

	if (base.find ("/") != string::npos) {
		base = base.substr (base.find_last_of ("/") + 1);
	}

	if (base == "") {
		snprintf (buf, sizeof (buf), "%d", RegionFactory::nregions () + 1);
		result = "region.";
		result += buf;

	} else {
		if (newlevel) {
			subbase = base;
		} else {
			string::size_type pos;

			pos = base.find_last_of ('.');

			/* pos may be npos, but then we just use entire base */

			subbase = base.substr (0, pos);
		}

		{
			Glib::Threads::Mutex::Lock lm (region_name_maps_mutex);

			map<string, uint32_t>::iterator x;

			result = subbase;

			if ((x = region_name_number_map.find (subbase)) == region_name_number_map.end ()) {
				result += ".1";
				region_name_number_map[subbase] = 1;
			} else {
				x->second++;
				snprintf (buf, sizeof (buf), ".%d", x->second);

				result += buf;
			}
		}
	}

	return 0;
}

string
RegionFactory::compound_region_name (const string& playlist, uint32_t compound_ops, uint32_t depth, bool whole_source)
{
	if (whole_source) {
		return string_compose (_("%1 compound-%2 (%3)"), playlist, compound_ops + 1, depth + 1);
	} else {
		return string_compose (_("%1 compound-%2.1 (%3)"), playlist, compound_ops + 1, depth + 1);
	}
}

string
RegionFactory::new_region_name (string old)
{
	string::size_type last_period;
	uint32_t          number;
	string::size_type len = old.length () + 64;
	string            remainder;
	std::vector<char> buf (len);

	if ((last_period = old.find_last_of ('.')) == string::npos) {
		/* no period present - add one explicitly */

		old += '.';
		last_period = old.length () - 1;
		number      = 0;

	} else {
		if (last_period < old.length () - 1) {
			string period_to_end = old.substr (last_period + 1);

			/* extra material after the period */

			string::size_type numerals_end = period_to_end.find_first_not_of ("0123456789");

			number = atoi (period_to_end);

			if (numerals_end < period_to_end.length () - 1) {
				/* extra material after the end of the digits */
				remainder = period_to_end.substr (numerals_end);
			}

		} else {
			last_period = old.length ();
			number      = 0;
		}
	}

	while (number < (UINT_MAX - 1)) {
		string sbuf;

		number++;

		snprintf (&buf[0], len, "%s%" PRIu32 "%s", old.substr (0, last_period + 1).c_str (), number, remainder.c_str ());
		sbuf = &buf[0];

		if (region_name_map.find (sbuf) == region_name_map.end ()) {
			break;
		}
	}

	if (number != (UINT_MAX - 1)) {
		return &buf[0];
	}

	error << string_compose (_("cannot create new name for region \"%1\""), old) << endmsg;
	return old;
}

boost::shared_ptr<Region>
RegionFactory::get_whole_region_for_source (boost::shared_ptr<Source> s)
{
	Glib::Threads::Mutex::Lock lm (region_map_lock);

	for (RegionMap::const_iterator i = region_map.begin (); i != region_map.end (); ++i) {
		if (i->second->uses_source (s) && i->second->whole_file ()) {
			return (i->second);
		}
	}

	return boost::shared_ptr<Region> ();
}

void
RegionFactory::get_regions_using_source (boost::shared_ptr<Source> s, std::set<boost::shared_ptr<Region> >& r)
{
	Glib::Threads::Mutex::Lock lm (region_map_lock);

	for (RegionMap::const_iterator i = region_map.begin (); i != region_map.end (); ++i) {
		if (i->second->uses_source (s)) {
			r.insert (i->second);
		}
	}
}

void
RegionFactory::remove_regions_using_source (boost::shared_ptr<Source> src)
{
	Glib::Threads::Mutex::Lock lm (region_map_lock);
	RegionList                 remove_regions;
	for (RegionMap::const_iterator i = region_map.begin (); i != region_map.end (); ++i) {
		if (i->second->uses_source (src)) {
			remove_regions.push_back (i->second);
		}
	}
	lm.release ();

	/* this will call RegionFactory::map_remove () */
	for (RegionList::iterator i = remove_regions.begin (); i != remove_regions.end (); ++i) {
		(*i)->drop_references ();
	}
}

void
RegionFactory::add_compound_association (boost::shared_ptr<Region> orig, boost::shared_ptr<Region> copy)
{
	Glib::Threads::Mutex::Lock lm (region_map_lock);
	_compound_associations[copy] = orig;
}
