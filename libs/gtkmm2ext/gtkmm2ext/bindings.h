/*
 * Copyright (C) 2010-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __libgtkmm2ext_bindings_h__
#define __libgtkmm2ext_bindings_h__

#include <map>
#include <vector>
#include <list>

#include <stdint.h>

#include <gdk/gdkkeysyms.h>
#include <gtkmm/action.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/toggleaction.h>

#include "pbd/signals.h"

#include "gtkmm2ext/visibility.h"

class XMLNode;
class XMLProperty;

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API KeyboardKey
{
  public:
	KeyboardKey () {
		_val = GDK_VoidSymbol;
	}

	KeyboardKey (uint32_t state, uint32_t keycode);

	static KeyboardKey null_key() { return KeyboardKey (0, 0); }

	uint32_t state() const { return _val >> 32; }
	uint32_t key() const { return _val & 0xffffffff; }

	bool operator<(const KeyboardKey& other) const {
		return _val < other._val;
	}

	bool operator==(const KeyboardKey& other) const {
		return _val == other._val;
	}

	std::string name() const;
	std::string native_name() const;
	std::string native_short_name() const;
	static bool make_key (const std::string&, KeyboardKey&);

	std::string display_label() const;

  private:
	uint64_t _val;
};

class LIBGTKMM2EXT_API MouseButton {
  public:
	MouseButton () {
		_val = ~0ULL;
	}

	MouseButton (uint32_t state, uint32_t button_number);
	uint32_t state() const { return _val >> 32; }
	uint32_t button() const { return _val & 0xffff; }

	bool operator<(const MouseButton& other) const {
		return _val < other._val;
	}

	bool operator==(const MouseButton& other) const {
		return _val == other._val;
	}

	std::string name() const;
	static bool make_button (const std::string&, MouseButton&);

  private:
	uint64_t _val;
};

class LIBGTKMM2EXT_API Bindings;

class LIBGTKMM2EXT_API Bindings {
  public:
	enum Operation {
		Press,
		Release
	};

	struct ActionInfo {
		ActionInfo (std::string const& name) : action_name (name) {}
		ActionInfo (std::string const& name, std::string const& grp) : action_name (name), group_name (grp) {}

		std::string action_name;
		std::string group_name; /* may be empty */
		mutable Glib::RefPtr<Gtk::Action> action;
	};
	typedef std::map<KeyboardKey,ActionInfo> KeybindingMap;

	Bindings (std::string const& name);
	~Bindings ();

	std::string const& name() const { return _name; }

	void reassociate ();
	void associate ();
	void dissociate ();

	bool empty() const;
	bool empty_keys () const;
	bool empty_mouse () const;

	bool add (KeyboardKey, Operation, std::string const&, XMLProperty const*, bool can_save = false);
	bool replace (KeyboardKey, Operation, std::string const& action_name, bool can_save = true);
	bool remove (Operation, std::string const& action_name, bool can_save = false);

	bool activate (KeyboardKey, Operation);

	void add (MouseButton, Operation, std::string const&, XMLProperty const*);
	void remove (MouseButton, Operation);
	bool activate (MouseButton, Operation);

	bool is_bound (KeyboardKey const&, Operation, std::string* path = 0) const;
	std::string bound_name (KeyboardKey const&, Operation) const;
	bool is_registered (Operation op, std::string const& action_name) const;

	KeyboardKey get_binding_for_action (Glib::RefPtr<Gtk::Action>, Operation& op);

	bool load (XMLNode const& node);
	void load_operation (XMLNode const& node);
	void save (XMLNode& root);
	void save_as_html (std::ostream&, bool) const;

	/* used for editing bindings */
	void get_all_actions (std::vector<std::string>& paths,
	                      std::vector<std::string>& labels,
	                      std::vector<std::string>& tooltips,
	                      std::vector<std::string>& keys,
	                      std::vector<Glib::RefPtr<Gtk::Action> >& actions);

	/* all bindings currently in existence, as grouped into Bindings */
	static void reset_bindings () { bindings.clear (); }
	static std::list<Bindings*> bindings;
	static Bindings* get_bindings (std::string const & name);
	static void associate_all ();
	static void save_all_bindings_as_html (std::ostream&);

	static PBD::Signal1<void,Bindings*> BindingsChanged;

  private:
	std::string  _name;
	KeybindingMap press_bindings;
	KeybindingMap release_bindings;

	typedef std::map<MouseButton,ActionInfo> MouseButtonBindingMap;
	MouseButtonBindingMap button_press_bindings;
	MouseButtonBindingMap button_release_bindings;

	void push_to_gtk (KeyboardKey, Glib::RefPtr<Gtk::Action>);

	KeybindingMap& get_keymap (Operation op);
	const KeybindingMap& get_keymap (Operation op) const;
	MouseButtonBindingMap& get_mousemap (Operation op);

	/* GTK has the following position a Gtk::Action:
	 *
	 *  accel_path: <Actions>/GroupName/ActionName
	 *  name: ActionName
	 *
	 * We want proper namespacing and we're not interested in
	 * the silly <Actions> "extra" namespace. So in Ardour:
	 *
	 * accel_path: <Actions>/GroupName/ActionName
	 * name: GroupName/ActionName
	 *
	 * This (static) method returns the "ardour" name for the action.
	 */
	static std::string ardour_action_name (Glib::RefPtr<Gtk::Action>);

};

} // namespace

std::ostream& operator<<(std::ostream& out, Gtkmm2ext::KeyboardKey const & k);

#endif /* __libgtkmm2ext_bindings_h__ */
