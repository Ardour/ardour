/*
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __pbd_controllable_h__
#define __pbd_controllable_h__

#include <string>
#include <set>

#include "pbd/libpbd_visibility.h"
#include "pbd/signals.h"
#include <glibmm/threads.h>

#include <boost/enable_shared_from_this.hpp>

#include "pbd/statefuldestructible.h"

class XMLNode;

namespace PBD {

/** This is a pure virtual class to represent a scalar control.
 *
 * Note that it contains no storage/state for the controllable thing that it
 * represents. Derived classes must provide set_value()/get_value() methods,
 * which will involve (somehow) an actual location to store the value.
 *
 * In essence, this is an interface, not a class.
 *
 * Without overriding upper() and lower(), a derived class will function
 * as a control whose value can range between 0 and 1.0.
 *
 *

 * We express Controllable values in one of three ways:
 * 1. `user' --- as presented to the user (e.g. dB, Hz, etc.)
 * 2. `interface' --- as used in some cases for the UI representation
 * (in order to make controls behave logarithmically).
 * 3. `internal' --- as passed to a processor, track, plugin, or whatever.
 *
 * Note that in some cases user and internal may be the same
 * (and interface different) e.g. frequency, which is presented
 * to the user and passed to the processor in linear terms, but
 * which needs log scaling in the interface.
 *
 * In other cases, user and interface may be the same (and internal different)
 * e.g. gain, which is presented to the user in log terms (dB)
 * but passed to the processor as a linear quantity.
 */
class LIBPBD_API Controllable : public PBD::StatefulDestructible, public boost::enable_shared_from_this<Controllable>
{
public:
	enum Flag {
		Toggle         = 0x01,
		GainLike       = 0x02,
		RealTime       = 0x04,
		NotAutomatable = 0x08,
		InlineControl  = 0x10,
		HiddenControl  = 0x20,
	};

	Controllable (const std::string& name, Flag f = Flag (0));

	/** Within an application, various Controllables might be considered to
	 * be "grouped" in a way that implies that setting 1 of them also
	 * modifies others in the group.
	 */
	enum GroupControlDisposition {
		InverseGroup,  /**< set all controls in the same "group" as this one */
		NoGroup,       /**< set only this control */
		UseGroup,      /**< use group settings to decide which group controls are altered */
		ForGroup       /**< this setting is being done *for* the group (i.e. UseGroup was set in the callchain somewhere). */
	};

	/** Set `internal' value
	 *
	 * All derived classes must implement this.
	 *
	 * Basic derived classes will ignore \p group_override
	 * but more sophisticated children, notably those that
	 * proxy the value setting logic via an object that is aware of group
	 * relationships between this control and others, will find it useful.
	 *
	 * @param value raw numeric value to set
	 * @param group_override if and how to propagate value to grouped controls
	 */
	virtual void set_value (double value, GroupControlDisposition group_override) = 0;

	/** Get `internal' value
	 * @return raw value as used for the plugin/processor control port
	 */
	virtual double get_value (void) const = 0;

	/** This is used when saving state. By default it just calls
	 * get_value(), but a class with more complex semantics might override
	 * this to save some value that differs from what get_value() would 
	 * return.
	 */
	virtual double get_save_value () const { return get_value(); }

	/** Conversions between `internal', 'interface', and 'user' values */
	virtual double internal_to_interface (double i, bool rotary = false) const {
		/* by default, the interface range is just a linear
		 * interpolation between lower and upper values */
		return  (i-lower())/(upper() - lower());
	}

	virtual double interface_to_internal (double i, bool rotary = false) const {
		return lower() + i*(upper() - lower());
	}

	/** Get and Set `interface' value  (typically, fraction of knob travel) */
	virtual float get_interface(bool rotary=false) const { return (internal_to_interface(get_value(), rotary)); }

	virtual void set_interface (float fraction, bool rotary=false, GroupControlDisposition gcd = NoGroup);

	virtual std::string get_user_string() const { return std::string(); }

	PBD::Signal0<void> LearningFinished;

	static PBD::Signal1<bool, boost::weak_ptr<PBD::Controllable> > StartLearning;
	static PBD::Signal1<void, boost::weak_ptr<PBD::Controllable> > StopLearning;

	static PBD::Signal1<void, boost::weak_ptr<PBD::Controllable> > GUIFocusChanged;
	static PBD::Signal1<void, boost::weak_ptr<PBD::Controllable> > ControlTouched;

	PBD::Signal2<void,bool,PBD::Controllable::GroupControlDisposition> Changed;

	int set_state (const XMLNode&, int version);
	virtual XMLNode& get_state ();

	std::string name() const { return _name; }

	bool touching () const { return _touching; }
	PBD::Signal0<void> TouchChanged;

	bool is_toggle() const { return _flags & Toggle; }
	bool is_gain_like() const { return _flags & GainLike; }

	virtual double lower() const { return 0.0; }
	virtual double upper() const { return 1.0; }
	virtual double normal() const { return 0.0; }  //the default value

	Flag flags() const { return _flags; }
	void set_flags (Flag f);

	void set_flag (Flag f); ///< _flags |= f;
	void clear_flag (Flag f); ///< _flags &= ~f;

	static boost::shared_ptr<Controllable> by_id (const PBD::ID&);
	static void dump_registry ();

	static const std::string xml_node_name;

protected:
	void set_touching (bool yn) {
		if (_touching == yn) { return; }
		_touching = yn;
		TouchChanged (); /* EMIT SIGNAL */
	}

private:
	std::string _name;
	std::string _units;
	Flag        _flags;
	bool        _touching;

	typedef std::set<PBD::Controllable*> Controllables;

	static ScopedConnectionList registry_connections;
	static Glib::Threads::RWLock registry_lock;
	static Controllables registry;

	static void add (Controllable&);
	static void remove (Controllable*);
};

}

#endif /* __pbd_controllable_h__ */
