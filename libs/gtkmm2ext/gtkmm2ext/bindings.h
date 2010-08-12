#ifndef __libgtkmm2ext_bindings_h__
#define __libgtkmm2ext_bindings_h__

#include <map>
#include <stdint.h>
#include <gdk/gdkkeysyms.h>
#include <gtkmm/action.h>
#include <gtkmm/action.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/toggleaction.h>

namespace Gtkmm2ext {

class KeyboardKey
{
  public:
        enum Operation { 
                Press,
                Release
        };

        KeyboardKey () {
                _val = GDK_VoidSymbol;
        }
        
        KeyboardKey (uint32_t state, uint32_t keycode);
        
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
        static void set_ignored_state (int mask) {
                _ignored_state = mask;
        }

  private:
        uint64_t _val;
        static uint32_t _ignored_state;
};

class ActionMap {
  public:
        ActionMap() {}
        ~ActionMap() {}

	Glib::RefPtr<Gtk::Action> register_action (const char* path,
						   const char* name, const char* label, sigc::slot<void> sl);
	Glib::RefPtr<Gtk::Action> register_radio_action (const char* path, Gtk::RadioAction::Group&,
							 const char* name, const char* label, 
                                                         sigc::slot<void,GtkAction*> sl,
                                                         int value);
	Glib::RefPtr<Gtk::Action> register_toggle_action (const char*path,
							  const char* name, const char* label, sigc::slot<void> sl);

        Glib::RefPtr<Gtk::Action> find_action (const std::string& name);

  private:
        typedef std::map<std::string, Glib::RefPtr<Gtk::Action> > _ActionMap;
        _ActionMap actions;
};        

class Bindings {
  public:
        Bindings();
        ~Bindings ();

        void add (KeyboardKey, KeyboardKey::Operation, Glib::RefPtr<Gtk::Action>);
        void remove (KeyboardKey, KeyboardKey::Operation);
        bool activate (KeyboardKey, KeyboardKey::Operation);

        bool load (const std::string& path);
        bool save (const std::string& path);
        
        void set_action_map (ActionMap&);

  private:
        typedef std::map<KeyboardKey,Glib::RefPtr<Gtk::Action> > KeybindingMap;
        KeybindingMap press_bindings;
        KeybindingMap release_bindings;

        ActionMap* action_map;
};

} // namespace

#endif /* __libgtkmm2ext_bindings_h__ */
