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

#ifndef __pbd_stateful_h__
#define __pbd_stateful_h__

#include <string>
#include <list>
#include <cassert>

#include "pbd/libpbd_visibility.h"
#include "pbd/id.h"
#include "pbd/xml++.h"
#include "pbd/property_basics.h"
#include "pbd/signals.h"
#include "pbd/g_atomic_compat.h"

class XMLNode;

namespace PBD {

namespace sys {
	class path;
}

class PropertyList;
class OwnedPropertyList;

/** Base class for objects with saveable and undoable state */
class LIBPBD_API Stateful {
  public:
	Stateful ();
	virtual ~Stateful();

	virtual XMLNode& get_state (void) = 0;
	virtual int set_state (const XMLNode&, int version) = 0;

	virtual bool apply_change (PropertyBase const &);
	PropertyChange apply_changes (PropertyList const &);

	const OwnedPropertyList& properties() const { return *_properties; }

	void add_property (PropertyBase& s);

	/* Extra XML node: so that 3rd parties can attach state to the XMLNode
	   representing the state of this object.
	 */

	void add_extra_xml (XMLNode&);
	XMLNode *extra_xml (const std::string& str, bool add_if_missing = false);
	void save_extra_xml (const XMLNode&);

	const PBD::ID& id() const { return _id; }
	bool set_id (const XMLNode&);
	void set_id (const std::string&);
	void reset_id ();

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

	void clear_changes ();
	virtual void clear_owned_changes ();
	PropertyList* get_changes_as_properties (Command *) const;
	virtual void rdiff (std::vector<Command*> &) const;
	bool changed() const;

	/* create a property list from an XMLNode */
	virtual PropertyList* property_factory (const XMLNode&) const;

	/* How stateful's notify of changes to their properties */
	PBD::Signal1<void,const PropertyChange&> PropertyChanged;

	static int current_state_version;
	static int loading_state_version;

	virtual void suspend_property_changes ();
	virtual void resume_property_changes ();

	bool property_changes_suspended() const { return g_atomic_int_get (const_cast<GATOMIC_QUAL gint*> (&_stateful_frozen)) > 0; }

  protected:

	void add_instant_xml (XMLNode&, const std::string& directory_path);
	XMLNode *instant_xml (const std::string& str, const std::string& directory_path);
	void add_properties (XMLNode &);

	PropertyChange set_values (XMLNode const &);

	/* derived classes can implement this to do cross-checking
	   of property values after either a PropertyList or XML
	   driven property change.
	*/
	virtual void post_set (const PropertyChange&) { };

	XMLNode *_extra_xml;
	XMLNode *_instant_xml;
	PBD::PropertyChange     _pending_changed;
	Glib::Threads::Mutex _lock;

	std::string _xml_node_name; ///< name of node to use for this object in XML
	OwnedPropertyList* _properties;

	virtual void send_change (const PropertyChange&);
	/** derived classes can implement this in order to process a property change
	    within thaw() just before send_change() is called.
	*/
	virtual void mid_thaw (const PropertyChange&) { }

	bool regenerate_xml_or_string_ids () const;

  private:
	friend struct ForceIDRegeneration;
	static Glib::Threads::Private<bool> _regenerate_xml_or_string_ids;

	PBD::ID           _id;
	GATOMIC_QUAL gint _stateful_frozen;

	static void set_regenerate_xml_and_string_ids_in_this_thread (bool yn);
};

} // namespace PBD

#endif /* __pbd_stateful_h__ */
