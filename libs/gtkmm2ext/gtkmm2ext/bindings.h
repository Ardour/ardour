#ifndef __libgtkmm2ext_bindings_h__
#define __libgtkmm2ext_bindings_h__

#include <map>
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
        void install_action_group (Glib::RefPtr<Gtk::ActionGroup>);
        
        Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, const char* name, const char* label);
        Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group,
						   const char* name, const char* label, sigc::slot<void> sl);
<<<<<<< HEAD
	Glib::RefPtr<Gtk::Action> register_radio_action (const char* path, Gtk::RadioAction::Group&,
							 const char* name, const char* label,
=======
        Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group,
							 Gtk::RadioAction::Group&,
							 const char* name, const char* label, 
>>>>>>> changes to Bindings and Keyboard API to support (mostly) GTK-free keyboard bindings
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
        typedef std::map<std::string, Glib::RefPtr<Gtk::Action> > _ActionMap;
        _ActionMap actions;
};

class LIBGTKMM2EXT_API Bindings {
  public:
        enum Operation {
                Press,
                Release
        };

        Bindings();
        ~Bindings ();

        bool empty() const;
        bool empty_keys () const;
        bool empty_mouse () const;
        
        void add (KeyboardKey, Operation, Glib::RefPtr<Gtk::Action>);
        void remove (KeyboardKey, Operation);
        bool activate (KeyboardKey, Operation);

        void add (MouseButton, Operation, Glib::RefPtr<Gtk::Action>);
        void remove (MouseButton, Operation);
        bool activate (MouseButton, Operation);

        bool load (const std::string& path);
        void load (const XMLNode& node);
        bool save (const std::string& path);
        void save (XMLNode& root);

        void set_action_map (ActionMap&);
        
        static void set_ignored_state (int mask) {
                _ignored_state = mask;
        }
        static uint32_t ignored_state() { return _ignored_state; }

        void get_all_actions (std::vector<std::string>& names,
                              std::vector<std::string>& paths,
                              std::vector<std::string>& tooltips,
                              std::vector<std::string>& keys,
                              std::vector<KeyboardKey>& bindings);
        
	void get_all_actions (std::vector<std::string>& groups,
	                      std::vector<std::string>& paths,
	                      std::vector<std::string>& tooltips,
	                      std::vector<KeyboardKey>& bindings);

  private:
        typedef std::map<KeyboardKey,Glib::RefPtr<Gtk::Action> > KeybindingMap;

        KeybindingMap press_bindings;
        KeybindingMap release_bindings;
        
        typedef std::map<MouseButton,Glib::RefPtr<Gtk::Action> > MouseButtonBindingMap;
        MouseButtonBindingMap button_press_bindings;
        MouseButtonBindingMap button_release_bindings;

        ActionMap* action_map;
        static uint32_t _ignored_state;
};

} // namespace

std::ostream& operator<<(std::ostream& out, Gtkmm2ext::KeyboardKey const & k);

#endif /* __libgtkmm2ext_bindings_h__ */
