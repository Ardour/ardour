/*
    Copyright (C) 2012 Paul Davis

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

#include <iostream>

#include "pbd/gstdio_compat.h"
#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/xml++.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/debug.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"

#include "i18n.h"

using namespace std;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

uint32_t Bindings::_ignored_state = 0;
map<string,Bindings*> Bindings::bindings_for_state;

MouseButton::MouseButton (uint32_t state, uint32_t keycode)
{
        uint32_t ignore = Bindings::ignored_state();

        if (gdk_keyval_is_upper (keycode) && gdk_keyval_is_lower (keycode)) {
                /* key is not subject to case, so ignore SHIFT
                 */
                ignore |= GDK_SHIFT_MASK;
        }

        _val = (state & ~ignore);
        _val <<= 32;
        _val |= keycode;
};

bool
MouseButton::make_button (const string& str, MouseButton& b)
{
        int s = 0;

        if (str.find ("Primary") != string::npos) {
                s |= Keyboard::PrimaryModifier;
        }

        if (str.find ("Secondary") != string::npos) {
                s |= Keyboard::SecondaryModifier;
        }

        if (str.find ("Tertiary") != string::npos) {
                s |= Keyboard::TertiaryModifier;
        }

        if (str.find ("Level4") != string::npos) {
                s |= Keyboard::Level4Modifier;
        }

        string::size_type lastmod = str.find_last_of ('-');
        uint32_t button_number;

        if (lastmod == string::npos) {
                button_number = PBD::atoi (str);
        } else {
                button_number = PBD::atoi (str.substr (lastmod+1));
        }

        b = MouseButton (s, button_number);
        return true;
}

string
MouseButton::name () const
{
        int s = state();

        string str;

        if (s & Keyboard::PrimaryModifier) {
                str += "Primary";
        }
        if (s & Keyboard::SecondaryModifier) {
                if (!str.empty()) {
                        str += '-';
                }
                str += "Secondary";
        }
        if (s & Keyboard::TertiaryModifier) {
                if (!str.empty()) {
                        str += '-';
                }
                str += "Tertiary";
        }
        if (s & Keyboard::Level4Modifier) {
                if (!str.empty()) {
                        str += '-';
                }
                str += "Level4";
        }

        if (!str.empty()) {
                str += '-';
        }

        char buf[16];
        snprintf (buf, sizeof (buf), "%u", button());
        str += buf;

        return str;
}

KeyboardKey::KeyboardKey (uint32_t state, uint32_t keycode)
{
        uint32_t ignore = Bindings::ignored_state();

        _val = (state & ~ignore);
        _val <<= 32;
        _val |= keycode;
};


string
KeyboardKey::name () const
{
        int s = state();

        string str;

        if (s & Keyboard::PrimaryModifier) {
                str += "Primary";
        }
        if (s & Keyboard::SecondaryModifier) {
                if (!str.empty()) {
                        str += '-';
                }
                str += "Secondary";
        }
        if (s & Keyboard::TertiaryModifier) {
                if (!str.empty()) {
                        str += '-';
                }
                str += "Tertiary";
        }
        if (s & Keyboard::Level4Modifier) {
                if (!str.empty()) {
                        str += '-';
                }
                str += "Level4";
        }

        if (!str.empty()) {
                str += '-';
        }

        str += gdk_keyval_name (key());

        return str;
}

bool
KeyboardKey::make_key (const string& str, KeyboardKey& k)
{
        int s = 0;

        if (str.find ("Primary") != string::npos) {
                s |= Keyboard::PrimaryModifier;
        }

        if (str.find ("Secondary") != string::npos) {
                s |= Keyboard::SecondaryModifier;
        }

        if (str.find ("Tertiary") != string::npos) {
                s |= Keyboard::TertiaryModifier;
        }

        if (str.find ("Level4") != string::npos) {
                s |= Keyboard::Level4Modifier;
        }

        string::size_type lastmod = str.find_last_of ('-');
        guint keyval;
        
        if (lastmod == string::npos) {
                keyval = gdk_keyval_from_name (str.c_str());
        } else {
                keyval = gdk_keyval_from_name (str.substr (lastmod+1).c_str());
        }

        if (keyval == GDK_VoidSymbol) {
	        return false;
        }

        k = KeyboardKey (s, keyval);

        return true;
}

Bindings::Bindings ()
        : action_map (0)
{
}

Bindings::~Bindings()
{
	if (!_name.empty()) {
		remove_bindings_for_state (_name, *this);
	}
}

bool
Bindings::empty_keys() const
{
	return press_bindings.empty() && release_bindings.empty();
}

bool
Bindings::empty_mouse () const
{
	return button_press_bindings.empty() && button_release_bindings.empty();
}

bool
Bindings::empty() const
{
	return empty_keys() && empty_mouse ();
}

void
Bindings::set_action_map (ActionMap& am)
{
        action_map = &am;
        press_bindings.clear ();
        release_bindings.clear ();
}

bool
Bindings::activate (KeyboardKey kb, Operation op)
{
        KeybindingMap* kbm = 0;

        switch (op) {
        case Press:
                kbm = &press_bindings;
                break;
        case Release:
                kbm = &release_bindings;
                break;
        }
        
        KeybindingMap::iterator k = kbm->find (kb);

        if (k == kbm->end()) {
                /* no entry for this key in the state map */
	        DEBUG_TRACE (DEBUG::Bindings, string_compose ("no binding for %1\n", kb));
	        return false;
        }

        /* lets do it ... */

        DEBUG_TRACE (DEBUG::Bindings, string_compose ("binding for %1: %2\n", kb, k->second->get_name()));

        k->second->activate ();
        return true;
}

bool
Bindings::replace (KeyboardKey kb, Operation op, string const & action_name, bool can_save)
{
	if (!action_map) {
		return false;
	}

	/* We have to search the existing binding map by both action and
	 * keybinding, because the following are possible:
	 *
	 *   - key is already used for a different action
	 *   - action has a different binding
	 *   - key is not used
	 *   - action is not bound
	 */
	
	RefPtr<Action> action = action_map->find_action (action_name);

        if (!action) {
	        return false;
        }

        KeybindingMap* kbm = 0;

        switch (op) {
        case Press:
                kbm = &press_bindings;
                break;
        case Release:
                kbm = &release_bindings;
                break;
        }

        KeybindingMap::iterator k = kbm->find (kb);

        if (k != kbm->end()) {
	        kbm->erase (k);
        }

        /* now linear search by action */

        for (k = kbm->begin(); k != kbm->end(); ++k) {
	        if (k->second == action) {
		        kbm->erase (k);
		        break;
	        }
        }
        
        add (kb, op, action, can_save);

        /* for now, this never fails */
        
        return true;
}

void
Bindings::add (KeyboardKey kb, Operation op, RefPtr<Action> what, bool can_save)
{
        KeybindingMap* kbm = 0;

        switch (op) {
        case Press:
	        kbm = &press_bindings;
                break;
        case Release:
                kbm = &release_bindings;
                break;
        }

        KeybindingMap::iterator k = kbm->find (kb);

        if (k == kbm->end()) {
	        pair<KeyboardKey,RefPtr<Action> > newpair (kb, what);
                kbm->insert (newpair);
        } else {
                k->second = what;
        }

        /* GTK has the useful feature of showing key bindings for actions in
         * menus. As of August 2015, we have no interest in trying to
         * reimplement this functionality, so we will use it even though we no
         * longer use GTK accelerators for handling key events. To do this, we
         * need to make sure that there is a fully populated GTK AccelMap set
         * up with all bindings/actions. 
         */

        Gtk::AccelKey gtk_key;

        /* tweak the modifier used in the binding so that GTK will accept it
         * and display something acceptable. The actual keyval should display
         * correctly even if it involves a key that GTK would not allow
         * as an accelerator.
         */

        uint32_t gtk_legal_keyval = kb.key();
        possibly_translate_keyval_to_make_legal_accelerator (gtk_legal_keyval);
        KeyboardKey gtk_binding (kb.state(), gtk_legal_keyval);
        

        bool entry_exists = Gtk::AccelMap::lookup_entry (what->get_accel_path(), gtk_key);

        if (!entry_exists || gtk_key.get_key() == 0) {

	        /* there is a trick happening here. It turns out that
	         * gtk_accel_map_add_entry() performs no validation checks on
	         * the accelerator keyval. This means we can use it to define
	         * ANY accelerator, even if they violate GTK's rules
	         * (e.g. about not using navigation keys). This works ONLY when
	         * the entry in the GTK accelerator map has not already been
	         * added. The entries will be added by the GTK UIManager when
	         * building menus, so this code must be called before that
	         * happens.
	         */

	        Gtk::AccelMap::add_entry (what->get_accel_path(),
	                                  gtk_binding.key(),
	                                  (Gdk::ModifierType) gtk_binding.state());
        } else {
	        warning << string_compose (_("There is more than one key binding defined for %1. Both will work, but only the first will be visible in menus"), what->get_accel_path()) << endmsg;
        }

        if (!Gtk::AccelMap::lookup_entry (what->get_accel_path(), gtk_key) || gtk_key.get_key() == 0) {
	        cerr << "GTK binding using " << gtk_binding << " failed for " << what->get_accel_path() << " existing = " << gtk_key.get_key() << " + " << gtk_key.get_mod() << endl;
        }

        if (can_save) {
	        Keyboard::save_keybindings ();
        }
}

void
Bindings::remove (KeyboardKey kb, Operation op, bool can_save)
{
        KeybindingMap* kbm = 0;

        switch (op) {
        case Press:
                kbm = &press_bindings;
                break;
        case Release:
                kbm = &release_bindings;
                break;
        }

        KeybindingMap::iterator k = kbm->find (kb);

        if (k != kbm->end()) {
                kbm->erase (k);
        }

        if (can_save) {
	        Keyboard::save_keybindings ();
        }
}

void
Bindings::remove (RefPtr<Action> action, Operation op, bool can_save)
{
        KeybindingMap* kbm = 0;

        switch (op) {
        case Press:
                kbm = &press_bindings;
                break;
        case Release:
                kbm = &release_bindings;
                break;
        }

        for (KeybindingMap::iterator k = kbm->begin(); k != kbm->end(); ++k) {
	        if (k->second == action) {
		        kbm->erase (k);
		        break;
	        }
        }

        if (can_save) {
	        Keyboard::save_keybindings ();
        }
}

bool
Bindings::activate (MouseButton bb, Operation op)
{
        MouseButtonBindingMap* bbm = 0;

        switch (op) {
        case Press:
                bbm = &button_press_bindings;
                break;
        case Release:
                bbm = &button_release_bindings;
                break;
        }

        MouseButtonBindingMap::iterator b = bbm->find (bb);

        if (b == bbm->end()) {
                /* no entry for this key in the state map */
                return false;
        }

        /* lets do it ... */

        b->second->activate ();
        return true;
}

void
Bindings::add (MouseButton bb, Operation op, RefPtr<Action> what)
{
        MouseButtonBindingMap* bbm = 0;

        switch (op) {
        case Press:
                bbm = &button_press_bindings;
                break;
        case Release:
                bbm = &button_release_bindings;
                break;
        }

        MouseButtonBindingMap::iterator b = bbm->find (bb);

        if (b == bbm->end()) {
                pair<MouseButton,RefPtr<Action> > newpair (bb, what);
                bbm->insert (newpair);
                // cerr << "Bindings added mouse button " << bb.button() << " w/ " << bb.state() << " => " << what->get_name() << endl;
        } else {
                b->second = what;
        }
}

void
Bindings::remove (MouseButton bb, Operation op)
{
        MouseButtonBindingMap* bbm = 0;

        switch (op) {
        case Press:
                bbm = &button_press_bindings;
                break;
        case Release:
                bbm = &button_release_bindings;
                break;
        }

        MouseButtonBindingMap::iterator b = bbm->find (bb);

        if (b != bbm->end()) {
                bbm->erase (b);
        }
}

bool
Bindings::save (const string& path)
{
        XMLTree tree;
        XMLNode* root = new XMLNode (X_("Bindings"));
        tree.set_root (root);

        save (*root);

        if (!tree.write (path)) {
                ::g_unlink (path.c_str());
                return false;
        }

        return true;
}

void
Bindings::save (XMLNode& root)
{
        XMLNode* presses = new XMLNode (X_("Press"));
        root.add_child_nocopy (*presses);

        for (KeybindingMap::iterator k = press_bindings.begin(); k != press_bindings.end(); ++k) {
                XMLNode* child;
                child = new XMLNode (X_("Binding"));
                child->add_property (X_("key"), k->first.name());
                string ap = k->second->get_accel_path();
                child->add_property (X_("action"), ap.substr (ap.find ('/') + 1));
                presses->add_child_nocopy (*child);
        }

        for (MouseButtonBindingMap::iterator k = button_press_bindings.begin(); k != button_press_bindings.end(); ++k) {
                XMLNode* child;
                child = new XMLNode (X_("Binding"));
                child->add_property (X_("button"), k->first.name());
                string ap = k->second->get_accel_path();
                child->add_property (X_("action"), ap.substr (ap.find ('/') + 1));
                presses->add_child_nocopy (*child);
        }

        XMLNode* releases = new XMLNode (X_("Release"));
        root.add_child_nocopy (*releases);

        for (KeybindingMap::iterator k = release_bindings.begin(); k != release_bindings.end(); ++k) {
                XMLNode* child;
                child = new XMLNode (X_("Binding"));
                child->add_property (X_("key"), k->first.name());
                string ap = k->second->get_accel_path();
                child->add_property (X_("action"), ap.substr (ap.find ('/') + 1));
                releases->add_child_nocopy (*child);
        }

        for (MouseButtonBindingMap::iterator k = button_release_bindings.begin(); k != button_release_bindings.end(); ++k) {
                XMLNode* child;
                child = new XMLNode (X_("Binding"));
                child->add_property (X_("button"), k->first.name());
                string ap = k->second->get_accel_path();
                child->add_property (X_("action"), ap.substr (ap.find ('/') + 1));
                releases->add_child_nocopy (*child);
        }

}

bool
Bindings::load (string const & name)
{
        XMLTree tree;

        if (!action_map) {
                return false;
        }

        XMLNode const * node = Keyboard::bindings_node();

        if (!node) {
	        error << string_compose (_("No keyboard binding information when loading bindings for \"%1\""), name) << endmsg;
	        return false;
        }

        if (!_name.empty()) {
	        remove_bindings_for_state (_name, *this);
        }
        
        const XMLNodeList& children (node->children());
        bool found = false;
        
        for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {

	        if ((*i)->name() == X_("Bindings")) {
		        XMLProperty const * prop = (*i)->property (X_("name"));

		        if (!prop) {
			        continue;
		        }

		        if (prop->value() == name) {
			        found = true;
			        node = *i;
			        break;
		        }
	        }
        }
        
        if (!found) {
	        error << string_compose (_("Bindings for \"%1\" not found in keyboard binding node\n"), name) << endmsg;
	        return false;
        }

        press_bindings.clear ();
        release_bindings.clear ();

        const XMLNodeList& bindings (node->children());

        for (XMLNodeList::const_iterator i = bindings.begin(); i != bindings.end(); ++i) {
	        /* each node could be Press or Release */
	        load (**i);
        }

        add_bindings_for_state (_name, *this);
        _name = name;
        
        return true;
}

void
Bindings::load (const XMLNode& node)
{
        if (node.name() == X_("Press") || node.name() == X_("Release")) {

                Operation op;

                if (node.name() == X_("Press")) {
                        op = Press;
                } else {
                        op = Release;
                }

                const XMLNodeList& children (node.children());

                for (XMLNodeList::const_iterator p = children.begin(); p != children.end(); ++p) {

                        XMLProperty* ap;
                        XMLProperty* kp;
                        XMLProperty* bp;

                        ap = (*p)->property ("action");
                        kp = (*p)->property ("key");
                        bp = (*p)->property ("button");

                        if (!ap || (!kp && !bp)) {
                                continue;
                        }

                        RefPtr<Action> act;

                        if (action_map) {
	                        act = action_map->find_action (ap->value());
                        } 

                        if (!act) {
                                string::size_type slash = ap->value().find ('/');
                                if (slash != string::npos) {
                                        string group = ap->value().substr (0, slash);
                                        string action = ap->value().substr (slash+1);
                                        act = ActionManager::get_action (group.c_str(), action.c_str());
                                }
                        }

                        if (!act) {
                                continue;
                        }

                        if (kp) {
                                KeyboardKey k;
                                if (!KeyboardKey::make_key (kp->value(), k)) {
                                        continue;
                                }
                                add (k, op, act);
                        } else {
                                MouseButton b;
                                if (!MouseButton::make_button (bp->value(), b)) {
                                        continue;
                                }
                                add (b, op, act);
                        }
                }
        }
}

void
Bindings::get_all_actions (std::vector<std::string>& paths,
                           std::vector<std::string>& labels,
                           std::vector<std::string>& tooltips,
                           std::vector<std::string>& keys,
                           std::vector<RefPtr<Action> >& actions)
{
	if (!action_map) {
		return;
	}
	
	/* build a reverse map from actions to bindings */

	typedef map<Glib::RefPtr<Gtk::Action>,KeyboardKey> ReverseMap;
	ReverseMap rmap;

	for (KeybindingMap::const_iterator k = press_bindings.begin(); k != press_bindings.end(); ++k) {
		rmap.insert (make_pair (k->second, k->first));
	}

	/* get a list of all actions */

	ActionMap::Actions all_actions;
	action_map->get_actions (all_actions);
	
	for (ActionMap::Actions::const_iterator act = all_actions.begin(); act != all_actions.end(); ++act) {

		paths.push_back ((*act)->get_accel_path());
		labels.push_back ((*act)->get_label());
		tooltips.push_back ((*act)->get_tooltip());

		ReverseMap::iterator r = rmap.find (*act);

		if (r != rmap.end()) {
			keys.push_back (gtk_accelerator_get_label (r->second.key(), (GdkModifierType) r->second.state()));
		} else {
			keys.push_back (string());
		}

		actions.push_back (*act);
	}
}

void
Bindings::get_all_actions (std::vector<std::string>& names,
                           std::vector<std::string>& paths,
                           std::vector<std::string>& keys)
{
	/* build a reverse map from actions to bindings */

	typedef map<Glib::RefPtr<Gtk::Action>,KeyboardKey> ReverseMap;
	ReverseMap rmap;

	for (KeybindingMap::const_iterator k = press_bindings.begin(); k != press_bindings.end(); ++k) {
		rmap.insert (make_pair (k->second, k->first));
	}

	/* get a list of all actions */

	ActionMap::Actions actions;
	action_map->get_actions (actions);
	
	for (ActionMap::Actions::const_iterator act = actions.begin(); act != actions.end(); ++act) {
		names.push_back ((*act)->get_name());
		paths.push_back ((*act)->get_accel_path());

		ReverseMap::iterator r = rmap.find (*act);
		if (r != rmap.end()) {
			keys.push_back (gtk_accelerator_get_label (r->second.key(), (GdkModifierType) r->second.state()));
		} else {
			keys.push_back (string());
		}
	}
}

void
ActionMap::get_actions (ActionMap::Actions& acts)
{
	for (_ActionMap::iterator a = actions.begin(); a != actions.end(); ++a) {
		acts.push_back (a->second);
	}
}

RefPtr<Action>
ActionMap::find_action (const string& name)
{
        _ActionMap::iterator a = actions.find (name);

        if (a != actions.end()) {
                return a->second;
        }

        return RefPtr<Action>();
}

RefPtr<ActionGroup>
ActionMap::create_action_group (const string& name)
{
	RefPtr<ActionGroup> g = ActionGroup::create (name);
	return g;
}

void
ActionMap::install_action_group (RefPtr<ActionGroup> group)
{
	ActionManager::ui_manager->insert_action_group (group);
}

RefPtr<Action> 
ActionMap::register_action (RefPtr<ActionGroup> group, const char* name, const char* label)
{
        string fullpath;

        RefPtr<Action> act = Action::create (name, label);

        fullpath = group->get_name();
        fullpath += '/';
        fullpath += name;
        
        if (actions.insert (_ActionMap::value_type (fullpath, act)).second) {
	        group->add (act);
	        return act;
        }

        /* already registered */
        return RefPtr<Action> ();
}

RefPtr<Action> 
ActionMap::register_action (RefPtr<ActionGroup> group,
                            const char* name, const char* label, sigc::slot<void> sl)
{
        string fullpath;

        RefPtr<Action> act = Action::create (name, label);

        fullpath = group->get_name();
        fullpath += '/';
        fullpath += name;

        if (actions.insert (_ActionMap::value_type (fullpath, act)).second) {
	        group->add (act, sl);
	        return act;
        }

        /* already registered */
        return RefPtr<Action>();
}

RefPtr<Action> 
ActionMap::register_radio_action (RefPtr<ActionGroup> group,
                                  Gtk::RadioAction::Group& rgroup,
                                  const char* name, const char* label, 
                                  sigc::slot<void> sl)
{
        string fullpath;

        RefPtr<Action> act = RadioAction::create (rgroup, name, label);
        RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
        
        fullpath = group->get_name();
        fullpath += '/';
        fullpath += name;

        if (actions.insert (_ActionMap::value_type (fullpath, act)).second) {
	        group->add (act, sl);
	        return act;
        }

        /* already registered */
        return RefPtr<Action>();
}

RefPtr<Action> 
ActionMap::register_radio_action (RefPtr<ActionGroup> group,
                                  Gtk::RadioAction::Group& rgroup,
                                  const char* name, const char* label, 
                                  sigc::slot<void,GtkAction*> sl,
                                  int value)
{
        string fullpath;

        RefPtr<Action> act = RadioAction::create (rgroup, name, label);
        RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
        ract->property_value() = value;

        fullpath = group->get_name();
        fullpath += '/';
        fullpath += name;

        if (actions.insert (_ActionMap::value_type (fullpath, act)).second) {
	        group->add (act, sigc::bind (sl, act->gobj()));
	        return act;
        }

        /* already registered */

        return RefPtr<Action>();
}

RefPtr<Action> 
ActionMap::register_toggle_action (RefPtr<ActionGroup> group,
                                   const char* name, const char* label, sigc::slot<void> sl)
{
        string fullpath;

        fullpath = group->get_name();
        fullpath += '/';
        fullpath += name;

        RefPtr<Action> act = ToggleAction::create (name, label);

        if (actions.insert (_ActionMap::value_type (fullpath, act)).second) {
	        group->add (act, sl);
	        return act;
        }

        /* already registered */
        return RefPtr<Action>();
}

void
Bindings::add_bindings_for_state (std::string const& name, Bindings& bindings)
{
	bindings_for_state.insert (make_pair (name, &bindings));
}

void
Bindings::remove_bindings_for_state (std::string const& name, Bindings& bindings)
{
	bindings_for_state.erase (name);
}

std::ostream& operator<<(std::ostream& out, Gtkmm2ext::KeyboardKey const & k) {
	char const *gdk_name = gdk_keyval_name (k.key());
	return out << "Key " << k.key() << " (" << (gdk_name ? gdk_name : "no-key") << ") state " << k.state();
}

