#ifndef __ardour_gtk_actions_h__
#define __ardour_gtk_actions_h__

#include <vector>
#include <gtkmm/action.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/actiongroup.h>
#include <gtkmm/accelkey.h>

namespace Gtk {
	class UIManager;
}

class ActionManager
{
  public:
	ActionManager() {}
	virtual ~ActionManager () {}

	static void init ();

	static std::vector<Glib::RefPtr<Gtk::Action> > session_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > region_list_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > region_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > track_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > plugin_selection_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > range_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > jack_sensitive_actions;
	static std::vector<Glib::RefPtr<Gtk::Action> > jack_opposite_sensitive_actions;

	static void set_sensitive (std::vector<Glib::RefPtr<Gtk::Action> >& actions, bool);

	static std::string unbound_string;  /* the key string returned if an action is not bound */
	static Glib::RefPtr<Gtk::UIManager> ui_manager;

	static Gtk::Widget* get_widget (Glib::ustring name);
	static Glib::RefPtr<Gtk::Action> get_action (Glib::ustring name);

	static void add_action_group (Glib::RefPtr<Gtk::ActionGroup>);

	static Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   std::string name, std::string label);
	static Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   std::string name, std::string label, sigc::slot<void> sl, 
						   guint key, Gdk::ModifierType mods);
	static Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   std::string name, std::string label, sigc::slot<void> sl);
	
	static Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group, 
							 std::string name, std::string label, sigc::slot<void> sl, 
							 guint key, Gdk::ModifierType mods);
	static Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group, 
							 std::string name, std::string label, sigc::slot<void> sl);
	
	static Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group, 
							  std::string name, std::string label, sigc::slot<void> sl, 
							  guint key, Gdk::ModifierType mods);
	static Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group, 
							  std::string name, std::string label, sigc::slot<void> sl);

	static bool lookup_entry (const Glib::ustring accel_path, Gtk::AccelKey& key);

	static void get_all_actions (std::vector<std::string>& names, 
				     std::vector<std::string>& paths, 
				     std::vector<std::string>& keys, 
				     std::vector<Gtk::AccelKey>& bindings);
	
};

#endif /* __ardour_gtk_actions_h__ */
