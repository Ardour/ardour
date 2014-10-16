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

#include <glib/gstdio.h>

#include "pbd/xml++.h"
#include "pbd/convert.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/keyboard.h"

#include "i18n.h"

using namespace std;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;

uint32_t Bindings::_ignored_state = 0;

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
        
        if (gdk_keyval_is_upper (keycode) && gdk_keyval_is_lower (keycode)) {
                /* key is not subject to case, so ignore SHIFT
                 */
                ignore |= GDK_SHIFT_MASK;
        }

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
                return false;
        }

        /* lets do it ... */

        k->second->activate ();
        return true;
}

void
Bindings::add (KeyboardKey kb, Operation op, RefPtr<Action> what)
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
                // cerr << "Bindings added " << kb.key() << " w/ " << kb.state() << " => " << what->get_name() << endl;
        } else {
                k->second = what;
        }
}

void
Bindings::remove (KeyboardKey kb, Operation op)
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
Bindings::load (const string& path)
{
        XMLTree tree;

        if (!action_map) {
                return false;
        }

        if (!tree.read (path)) {
                return false;
        }
        
        press_bindings.clear ();
        release_bindings.clear ();

        XMLNode& root (*tree.root());
        const XMLNodeList& children (root.children());

        for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
                load (**i);
        }

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

RefPtr<Action>
ActionMap::find_action (const string& name)
{
        _ActionMap::iterator a = actions.find (name);

        if (a != actions.end()) {
                return a->second;
        }

        return RefPtr<Action>();
}

RefPtr<Action> 
ActionMap::register_action (const char* path,
                            const char* name, const char* label, sigc::slot<void> sl)
{
        string fullpath;

        RefPtr<Action> act = Action::create (name, label);

        act->signal_activate().connect (sl);

        fullpath = path;
        fullpath += '/';
        fullpath += name;

        actions.insert (_ActionMap::value_type (fullpath, act));
        return act;
}

RefPtr<Action> 
ActionMap::register_radio_action (const char* path, Gtk::RadioAction::Group& rgroup,
                                  const char* name, const char* label, 
                                  sigc::slot<void,GtkAction*> sl,
                                  int value)
{
        string fullpath;

        RefPtr<Action> act = RadioAction::create (rgroup, name, label);
        RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
        ract->property_value() = value;

        act->signal_activate().connect (sigc::bind (sl, act->gobj()));

        fullpath = path;
        fullpath += '/';
        fullpath += name;

        actions.insert (_ActionMap::value_type (fullpath, act));
        return act;
}

RefPtr<Action> 
ActionMap::register_toggle_action (const char* path,
                                   const char* name, const char* label, sigc::slot<void> sl)
{
        string fullpath;

        RefPtr<Action> act = ToggleAction::create (name, label);

        act->signal_activate().connect (sl);

        fullpath = path;
        fullpath += '/';
        fullpath += name;

        actions.insert (_ActionMap::value_type (fullpath, act));
        return act;
}
