#ifndef __gtk2_ardour_auplugin_ui_h__
#define __gtk2_ardour_auplugin_ui_h__

#include <vector>
#include <string>

#define WITH_CARBON

#ifdef WITH_CARBON
#include <Carbon/Carbon.h>
#include <AudioUnit/AudioUnitCarbonView.h>
#include <AppKit/AppKit.h>
#endif

#include <AudioUnit/AudioUnit.h>

/* fix up stupid apple macros */

#undef check
#undef require
#undef verify

#include <gtkmm/box.h>
#include <gtkmm/combobox.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include "plugin_ui.h"

namespace ARDOUR {
	class AUPlugin;
	class PluginInsert;
	class Redirect;
}

class AUPluginUI;

@interface NotificationObject : NSObject {
	@private
		AUPluginUI* plugin_ui;
	        NSWindow* cocoa_parent;
		NSWindow* top_level_parent;
}
@end

class AUPluginUI : public PlugUIBase, public Gtk::VBox
{
  public:
	AUPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>);
	~AUPluginUI ();
	
	gint get_preferred_height () { return prefheight; }
	gint get_preferred_width () { return prefwidth; }
	bool start_updating(GdkEventAny*);
	bool stop_updating(GdkEventAny*);
	
	void activate ();
	void deactivate ();

	bool on_window_show (const std::string&);
	void on_window_hide ();

        bool non_gtk_gui() const { return true; }

	void lower_box_realized ();
	void cocoa_view_resized ();
	void on_realize ();
	bool on_map_event (GdkEventAny*);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);
	void forward_key_event (GdkEventKey*);

#ifdef WITH_CARBON
	OSStatus carbon_event (EventHandlerCallRef nextHandlerRef, EventRef event);
#endif

  private:
	boost::shared_ptr<ARDOUR::AUPlugin> au;
	int prefheight;
	int prefwidth;

	Gtk::HBox  top_box;
	Gtk::HBox  low_box;
	Gtk::VBox  vpacker;
	Gtk::Label automation_mode_label;
	Gtk::ComboBoxText automation_mode_selector;
	Gtk::Label preset_label;

	static std::vector<std::string> automation_mode_strings;

	/* Cocoa */

	NSWindow* cocoa_parent;
	NSWindow* cocoa_window;
	NSView*   au_view;

	bool test_cocoa_view_support ();
	int  create_cocoa_view ();
	int  parent_cocoa_window ();

	NotificationObject* _notify;

#ifdef WITH_CARBON
	/* Carbon */

	ComponentDescription carbon_descriptor;
	AudioUnitCarbonView  editView;
	WindowRef            carbon_window;	
 	EventHandlerRef      carbon_event_handler;
	bool                 _activating_from_app;

#endif

	bool test_carbon_view_support ();
	int  create_carbon_view ();
	int  parent_carbon_window ();

	NSWindow* get_nswindow();

	bool plugin_class_valid (Class pluginClass);
};

#endif /* __gtk2_ardour_auplugin_ui_h__  */
