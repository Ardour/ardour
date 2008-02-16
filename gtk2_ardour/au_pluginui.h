#ifndef __gtk2_ardour_auplugin_ui_h__
#define __gtk2_ardour_auplugin_ui_h__

#include <vector>
#include <string>

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <AudioUnit/AudioUnitCarbonView.h>
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
	class IOProcessor;
}

class AUPluginUI : public PlugUIBase, public Gtk::VBox
{
  public:
	AUPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>);
	~AUPluginUI ();
	
	gint get_preferred_height () { return prefheight; }
	gint get_preferred_width () { return prefwidth; }
	bool start_updating(GdkEventAny*);
	bool stop_updating(GdkEventAny*);
	
	virtual void activate ();
	virtual void deactivate ();
	
	void lower_box_realized ();
	void on_realize ();
	void on_show ();
	void on_hide ();
	bool on_map_event (GdkEventAny*);
	bool on_focus_in_event (GdkEventFocus*);
	bool on_focus_out_event (GdkEventFocus*);

	OSStatus carbon_event (EventHandlerCallRef nextHandlerRef, EventRef event);

  private:
	WindowRef wr;
	boost::shared_ptr<ARDOUR::AUPlugin> au;
	int prefheight;
	int prefwidth;
	
	Gtk::HBox     top_box;
	Gtk::EventBox low_box;
	Gtk::VBox vpacker;
	Gtk::Label automation_mode_label;
	Gtk::ComboBoxText automation_mode_selector;
	Gtk::Label preset_label;

	static std::vector<std::string> automation_mode_strings;

	/* Cocoa */

	NSWindow*           cocoa_window;
	NSScrollView*       scroll_view;
	NSView*             au_view;

	/* Carbon */

	NSWindow*            cocoa_parent;
	ComponentDescription carbon_descriptor;
	AudioUnitCarbonView  editView;
	WindowRef            carbon_window;	
 	EventHandlerRef      carbon_event_handler;
	bool                 _activating_from_app;

	bool test_cocoa_view_support ();
	bool test_carbon_view_support ();
	int  create_carbon_view ();
	int  create_cocoa_view ();

	int parent_carbon_window ();
	int parent_cocoa_window ();
	NSWindow* get_nswindow();

	bool plugin_class_valid (Class pluginClass);
};

#endif /* __gtk2_ardour_auplugin_ui_h__  */
