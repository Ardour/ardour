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

class LIBGTKMM2EXT_API ActionMap {
  public:
	ActionMap (std::string const& name);
	~ActionMap();

	std::string name() const { return _name; }

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

	void set_bindings (Bindings*);
	Bindings* bindings() const { return _bindings; }

	typedef std::vector<Glib::RefPtr<Gtk::Action> > Actions;
	void get_actions (Actions&);

	static std::list<ActionMap*> action_maps;

        /* used by control surface protocols and other UIs */
	static void get_all_actions (std::vector<std::string>& paths,
	                             std::vector<std::string>& labels,
	                             std::vector<std::string>& tooltips,
	                             std::vector<std::string>& keys,
	                             std::vector<Glib::RefPtr<Gtk::Action> >& actions);

  private:
	std::string _name;

	/* hash for faster lookup of actions by name */

	typedef std::map<std::string, Glib::RefPtr<Gtk::Action> > _ActionMap;
        _ActionMap _actions;

        /* initialized to null; set after a Bindings object has ::associated()
         * itself with this action map.
         */

        Bindings* _bindings;

};

class LIBGTKMM2EXT_API Bindings {
  public:
        enum Operation {
                Press,
                Release
        };

        struct ActionInfo {
	        ActionInfo (std::string const& name) : action_name (name) {}

	        std::string action_name;
	        Glib::RefPtr<Gtk::Action> action;
        };

        Bindings (std::string const& name);
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

        KeyboardKey get_binding_for_action (Glib::RefPtr<Gtk::Action>, Operation& op);

        bool load (XMLNode const& node);
        void load_operation (XMLNode const& node);
        void save (XMLNode& root);

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

        void set_action_map (ActionMap&);

        /* used for editing bindings */
        void get_all_actions (std::vector<std::string>& paths,
                              std::vector<std::string>& labels,
                              std::vector<std::string>& tooltips,
                              std::vector<std::string>& keys,
                              std::vector<Glib::RefPtr<Gtk::Action> >& actions);

	/* all bindings currently in existence, as grouped into Bindings */
	static std::list<Bindings*> bindings;
	static Bindings* get_bindings (std::string const& name, ActionMap&);
	static void associate_all ();

	static PBD::Signal1<void,Bindings*> BindingsChanged;

  private:
        typedef std::map<KeyboardKey,ActionInfo> KeybindingMap;

        std::string  _name;
        ActionMap*   _action_map;
        KeybindingMap press_bindings;
        KeybindingMap release_bindings;

        typedef std::map<MouseButton,ActionInfo> MouseButtonBindingMap;
        MouseButtonBindingMap button_press_bindings;
        MouseButtonBindingMap button_release_bindings;

        void push_to_gtk (KeyboardKey, Glib::RefPtr<Gtk::Action>);
};

} // namespace

std::ostream& operator<<(std::ostream& out, Gtkmm2ext::KeyboardKey const & k);

#endif /* __libgtkmm2ext_bindings_h__ */
