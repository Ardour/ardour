/*
 * Copyright (C) 2000-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2007-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <atomic>
#include <string>
#include <list>
#include <cassert>

#include "pbd/libpbd_visibility.h"
#include "pbd/id.h"
#include "pbd/xml++.h"
#include "pbd/property_basics.h"
#include "pbd/signals.h"

class XMLNode;

namespace PBD {

namespace sys {
	class path;
}

class PropertyList;
class OwnedPropertyList;

/** Base class for objects with saveable and undoable state */
class /*LIBPBD_API*/ Stateful {
  public:
	LIBPBD_API Stateful ();
	LIBPBD_API virtual ~Stateful();

	LIBPBD_API virtual XMLNode& get_state () const = 0;
	LIBPBD_API virtual int set_state (const XMLNode&, int version) = 0;

	LIBPBD_API virtual bool apply_change (PropertyBase const &);
	LIBPBD_API PropertyChange apply_changes (PropertyList const &);

	LIBPBD_API const OwnedPropertyList& properties() const { return *_properties; }

	LIBPBD_API void add_property (PropertyBase& s);

	/* Extra XML node: so that 3rd parties can attach state to the XMLNode
	   representing the state of this object.
	 */

	LIBPBD_API void add_extra_xml (XMLNode&);
	LIBPBD_API XMLNode *extra_xml (const std::string& str, bool add_if_missing = false);
	LIBPBD_API void save_extra_xml (const XMLNode&);

	LIBPBD_API const PBD::ID& id() const { return _id; }
	LIBPBD_API bool set_id (const XMLNode&);
	LIBPBD_API void set_id (const std::string&);
	LIBPBD_API void reset_id ();

	/* RAII structure to manage thread-local ID regeneration.
	 */
	struct ForceIDRegeneration {
		ForceIDRegeneration () {
			set_regenerate_xml_and_string_ids_in_this_thread (true);
		}
		~ForceIDRegeneration () {
			set_regenerate_xml_and_string_ids_in_this_thread (false);
		}
	};

	/* history management */

	LIBPBD_API void clear_changes ();
	LIBPBD_API virtual void clear_owned_changes ();
	LIBPBD_API PropertyList* get_changes_as_properties (PBD::Command *) const;
	LIBPBD_API virtual void rdiff (std::vector<PBD::Command*> &) const;
	LIBPBD_API bool changed() const;

	/* create a property list from an XMLNode */
	LIBPBD_API virtual PropertyList* property_factory (const XMLNode&) const;

	/* How stateful's notify of changes to their properties */
	/*LIBPBD_API*/ PBD::Signal<void(const PropertyChange&)> PropertyChanged;

	LIBPBD_API static int current_state_version;
	LIBPBD_API static int loading_state_version;

	LIBPBD_API virtual void suspend_property_changes ();
	LIBPBD_API virtual void resume_property_changes ();

	LIBPBD_API bool property_changes_suspended() const { return _stateful_frozen.load() > 0; }

  protected:

	LIBPBD_API void add_instant_xml (XMLNode&, const std::string& directory_path);
	LIBPBD_API XMLNode *instant_xml (const std::string& str, const std::string& directory_path);
	LIBPBD_API void add_properties (XMLNode &) const;

	LIBPBD_API PropertyChange set_values (XMLNode const &);

	/* derived classes can implement this to do cross-checking
	   of property values after either a PropertyList or XML
	   driven property change.
	*/
	LIBPBD_API virtual void post_set (const PropertyChange&) { };

	XMLNode *_extra_xml;
	XMLNode *_instant_xml;
	PBD::PropertyChange     _pending_changed;
	PBD::Mutex _lock;

	std::string _xml_node_name; ///< name of node to use for this object in XML
	OwnedPropertyList* _properties;

	LIBPBD_API virtual void send_change (const PropertyChange&);
	/** derived classes can implement this in order to process a property change
	    within thaw() just before send_change() is called.
	*/
	LIBPBD_API virtual void mid_thaw (const PropertyChange&) { }

	LIBPBD_API bool regenerate_xml_or_string_ids () const;

  private:
	friend struct ForceIDRegeneration;
	static thread_local bool _regenerate_xml_or_string_ids;

	PBD::ID           _id;
	std::atomic<int> _stateful_frozen;

	LIBPBD_API static void set_regenerate_xml_and_string_ids_in_this_thread (bool yn);
};

} // namespace PBD

