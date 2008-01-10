#ifndef __gtk2_ardour_auplugin_ui_h__
#define __gtk2_ardour_auplugin_ui_h__

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <AudioUnit/AudioUnit.h>

/* fix up stupid apple macros */

#undef check
#undef require
#undef verify

#include <gtkmm/box.h>
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

	void on_realize ();
	void on_show ();

	OSStatus carbon_event (EventHandlerCallRef nextHandlerRef, EventRef event);

  private:
	WindowRef wr;
	boost::shared_ptr<ARDOUR::AUPlugin> au;
	int prefheight;
	int prefwidth;

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
	bool                 carbon_parented;
	bool                 cocoa_parented;

	void test_view_support (bool&, bool&);
	bool test_cocoa_view_support ();
	bool test_carbon_view_support ();
	int  create_carbon_view (bool generic);
	int  create_cocoa_view ();

	int parent_carbon_window ();
	int parent_cocoa_window ();
	NSWindow* get_nswindow();

	bool plugin_class_valid (Class pluginClass);
};

#endif /* __gtk2_ardour_auplugin_ui_h__  */
