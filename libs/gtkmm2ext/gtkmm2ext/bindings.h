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

#include "gtkmm2ext/visibility.h"

class XMLNode;

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
        uint32_t key() const { return _val & 0xffff; }

        bool operator<(const KeyboardKey& other) const {
                return _val < other._val;
        }

        bool operator==(const KeyboardKey& other) const {
                return _val == other._val;
        }

        std::string name() const;
        static bool make_key (const std::string&, KeyboardKey&);

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

class LIBGTKMM2EXT_API ActionMap {
  public:
        ActionMap() {}
        ~ActionMap() {}

        Glib::RefPtr<Gtk::ActionGroup> create_action_group (const std::string& group_name);

        Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, const char* name, const char* label);
        Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group,
						   const char* name, const char* label, sigc::slot<void> sl);
        Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group,
							 Gtk::RadioAction::Group&,
							 const char* name, const char* label, 
                                                         sigc::slot<void,GtkAction*> sl,
                                                         int value);
        Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group,
							 Gtk::RadioAction::Group&,
							 const char* name, const char* label, 
                                                         sigc::slot<void> sl);
	Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group,
							  const char* name, const char* label, sigc::slot<void> sl);

        Glib::RefPtr<Gtk::Action> find_action (const std::string& name);

        typedef std::vector<Glib::RefPtr<Gtk::Action> > Actions;
	void get_actions (Actions&);

  private:
<<<<<<< HEAD
        typedef std::map<std::string, Glib::RefPtr<Gtk::Action> > _ActionMap;
        _ActionMap actions;
};
=======
	/* hash for faster lookup of actions by name */

	typedef std::map<std::string, Glib::RefPtr<Gtk::Action> > _ActionMap;
        _ActionMap _actions;
};        
>>>>>>> radically change Keyboard/Binding API design to disconnect Gtk::Action lookup from binding definition

/* single global action map for entire application. 
 * 
 * Actions are name-spaced by group, and it makes things
 * much easier if there is a single place to look up
 * any action.
 */

LIBGTKMM2EXT_API extern ActionMap Actions;

class LIBGTKMM2EXT_API Bindings {
  public:
        enum Operation {
                Press,
                Release
        };

<<<<<<< HEAD
        Bindings();
=======
        struct ActionInfo {
	        ActionInfo (std::string const& name) : action_name (name) {}

	        std::string action_name;
	        Glib::RefPtr<Gtk::Action> action;
        };
        
        Bindings (std::string const& name);
>>>>>>> radically change Keyboard/Binding API design to disconnect Gtk::Action lookup from binding definition
        ~Bindings ();

        std::string const& name() const { return _name; }

        void associate ();
        void dissociate ();
        
        bool empty() const;
        bool empty_keys () const;
        bool empty_mouse () const;
        
        void add (KeyboardKey, Operation, std::string const&, bool can_save = false);
        bool replace (KeyboardKey, Operation, std::string const& action_name, bool can_save = true);
        void remove (KeyboardKey, Operation, bool can_save = false);
        void remove (Glib::RefPtr<Gtk::Action>, Operation, bool can_save = false);

        bool activate (KeyboardKey, Operation);

        void add (MouseButton, Operation, std::string const&);
        void remove (MouseButton, Operation);
        bool activate (MouseButton, Operation);

        bool load (XMLNode const& node);
        void load_operation (XMLNode const& node);
        void save (XMLNode& root);
<<<<<<< HEAD

        void set_action_map (ActionMap&);
=======
>>>>>>> radically change Keyboard/Binding API design to disconnect Gtk::Action lookup from binding definition
        
        static void set_ignored_state (int mask) {
                _ignored_state = mask;
        }
        static uint32_t ignored_state() { return _ignored_state; }

        void set_action_map (ActionMap&);
        
        /* used to list all actions */
        void get_all_actions (std::vector<std::string>& names,
                              std::vector<std::string>& paths,
                              std::vector<std::string>& keys);

        /* used for editing bindings */
	void get_all_actions (std::vector<std::string>& paths,
	                      std::vector<std::string>& labels,
	                      std::vector<std::string>& tooltips,
	                      std::vector<std::string>& keys,
	                      std::vector<Glib::RefPtr<Gtk::Action> >& actions);

	/* all bindings currently in existence, as grouped into Bindings */
	static std::list<Bindings*> bindings;
	static Bindings* get_bindings (std::string const& name);
	static void associate_all ();
	
  private:
        typedef std::map<KeyboardKey,ActionInfo> KeybindingMap;

        std::string  _name;
        ActionMap&   _action_map;
        KeybindingMap press_bindings;
        KeybindingMap release_bindings;
        
        typedef std::map<MouseButton,ActionInfo> MouseButtonBindingMap;
        MouseButtonBindingMap button_press_bindings;
        MouseButtonBindingMap button_release_bindings;

        static uint32_t _ignored_state;

        void push_to_gtk (KeyboardKey, Glib::RefPtr<Gtk::Action>);
};

} // namespace

std::ostream& operator<<(std::ostream& out, Gtkmm2ext::KeyboardKey const & k);

#endif /* __libgtkmm2ext_bindings_h__ */
