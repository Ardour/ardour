#ifndef __ardour_gtk_actions_h__
#define __ardour_gtk_actions_h__

#include <vector>
#include <gtkmm/action.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/actiongroup.h>

namespace Gtk {
	class UIManager;
}

namespace ActionManager
{
	extern std::vector<Glib::RefPtr<Gtk::Action> > session_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > region_list_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > region_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > track_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > plugin_selection_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > range_sensitive_actions;
	extern std::vector<Glib::RefPtr<Gtk::Action> > jack_sensitive_actions;

	extern std::string unbound_string;  /* the key string returned if an action is not bound */

	void register_ui_manager (Glib::RefPtr<Gtk::UIManager>);

	Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   std::string name, std::string label);
	Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   std::string name, std::string label, sigc::slot<void> sl, 
						   guint key, Gdk::ModifierType mods);
	Glib::RefPtr<Gtk::Action> register_action (Glib::RefPtr<Gtk::ActionGroup> group, 
						   std::string name, std::string label, sigc::slot<void> sl);
	
	Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group, 
							 std::string name, std::string label, sigc::slot<void> sl, 
							 guint key, Gdk::ModifierType mods);
	Glib::RefPtr<Gtk::Action> register_radio_action (Glib::RefPtr<Gtk::ActionGroup> group, Gtk::RadioAction::Group, 
							 std::string name, std::string label, sigc::slot<void> sl);
	
	Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group, 
							  std::string name, std::string label, sigc::slot<void> sl, 
							  guint key, Gdk::ModifierType mods);
	Glib::RefPtr<Gtk::Action> register_toggle_action (Glib::RefPtr<Gtk::ActionGroup> group, 
							  std::string name, std::string label, sigc::slot<void> sl);

	void merge_actions (Glib::RefPtr<Gtk::ActionGroup> dst, const Glib::RefPtr<Gtk::ActionGroup> src);

};

#endif /* __ardour_gtk_actions_h__ */
