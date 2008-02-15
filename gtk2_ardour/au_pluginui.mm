#include <pbd/error.h>
#include <ardour/audio_unit.h>
#include <ardour/insert.h>

#undef check // stupid gtk, stupid apple

#include <gtkmm/button.h>
#include <gdk/gdkquartz.h>

#include <gtkmm2ext/utils.h>

#include "au_pluginui.h"
#include "gui_thread.h"

#include <appleutility/CAAudioUnit.h>
#include <appleutility/CAComponent.h>

#import <AudioUnit/AUCocoaUIView.h>
#import <CoreAudioKit/AUGenericView.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace sigc;
using namespace std;
using namespace PBD;

vector<string> AUPluginUI::automation_mode_strings;

static const gchar* _automation_mode_strings[] = {
	X_("Manual"),
	X_("Play"),
	X_("Write"),
	X_("Touch"),
	0
};

AUPluginUI::AUPluginUI (boost::shared_ptr<PluginInsert> insert)
	: PlugUIBase (insert)
	, automation_mode_label (_("Automation"))
	, preset_label (_("Presets"))
	
{
	if (automation_mode_strings.empty()) {
		automation_mode_strings = I18N (_automation_mode_strings);
	}
	
	set_popdown_strings (automation_mode_selector, automation_mode_strings);
	automation_mode_selector.set_active_text (automation_mode_strings.front());

	if ((au = boost::dynamic_pointer_cast<AUPlugin> (insert->plugin())) == 0) {
		error << _("unknown type of editor-supplying plugin (note: no AudioUnit support in this version of ardour)") << endmsg;
		throw failed_constructor ();
	}

	/* stuff some stuff into the top of the window */

	top_box.set_spacing (6);
	top_box.set_border_width (6);

	top_box.pack_end (bypass_button, false, true);
	top_box.pack_end (automation_mode_selector, false, false);
	top_box.pack_end (automation_mode_label, false, false);
	top_box.pack_end (save_button, false, false);
	top_box.pack_end (preset_combo, false, false);
	top_box.pack_end (preset_label, false, false);

	set_spacing (6);
	pack_start (top_box, false, false);
	pack_start (low_box, false, false);

	preset_label.show ();
	preset_combo.show ();
	automation_mode_label.show ();
	automation_mode_selector.show ();
	bypass_button.show ();
	top_box.show ();
	low_box.show ();

	_activating_from_app = false;
	cocoa_parent = 0;
	cocoa_window = 0;
	au_view = 0;

	/* prefer cocoa, fall back to cocoa, but use carbon if its there */

	if (test_cocoa_view_support()) {
		create_cocoa_view ();
	} else if (test_carbon_view_support()) {
		create_carbon_view ();
	} else {
		create_cocoa_view ();
	}

	low_box.signal_realize().connect (mem_fun (this, &AUPluginUI::lower_box_realized));
}

AUPluginUI::~AUPluginUI ()
{
	if (cocoa_parent) {
		NSWindow* win = get_nswindow();
		RemoveEventHandler(carbon_event_handler);
		[win removeChildWindow:cocoa_parent];
	} else if (carbon_window) {
		/* never parented */
		DisposeWindow (carbon_window);
	}

}

bool
AUPluginUI::test_carbon_view_support ()
{
	bool ret = false;
	
	carbon_descriptor.componentType = kAudioUnitCarbonViewComponentType;
	carbon_descriptor.componentSubType = 'gnrc';
	carbon_descriptor.componentManufacturer = 'appl';
	carbon_descriptor.componentFlags = 0;
	carbon_descriptor.componentFlagsMask = 0;
	
	OSStatus err;

	// ask the AU for its first editor component
	UInt32 propertySize;
	err = AudioUnitGetPropertyInfo(*au->get_au(), kAudioUnitProperty_GetUIComponentList, kAudioUnitScope_Global, 0, &propertySize, NULL);
	if (!err) {
		int nEditors = propertySize / sizeof(ComponentDescription);
		ComponentDescription *editors = new ComponentDescription[nEditors];
		err = AudioUnitGetProperty(*au->get_au(), kAudioUnitProperty_GetUIComponentList, kAudioUnitScope_Global, 0, editors, &propertySize);
		if (!err) {
			// just pick the first one for now
			carbon_descriptor = editors[0];
			ret = true;
		}
		delete[] editors;
	}

	return ret;
}
	
bool
AUPluginUI::test_cocoa_view_support ()
{
	UInt32 dataSize   = 0;
	Boolean isWritable = 0;
	OSStatus err = AudioUnitGetPropertyInfo(*au->get_au(),
						kAudioUnitProperty_CocoaUI, kAudioUnitScope_Global,
						0, &dataSize, &isWritable);
	
	return dataSize > 0 && err == noErr;
}

bool
AUPluginUI::plugin_class_valid (Class pluginClass)
{
	if([pluginClass conformsToProtocol: @protocol(AUCocoaUIBase)]) {
		if([pluginClass instancesRespondToSelector: @selector(interfaceVersion)] &&
		   [pluginClass instancesRespondToSelector: @selector(uiViewForAudioUnit:withSize:)]) {
				return true;
		}
	}
	return false;
}

int
AUPluginUI::create_cocoa_view ()
{
	BOOL wasAbleToLoadCustomView = NO;
	AudioUnitCocoaViewInfo* cocoaViewInfo = NULL;
	UInt32               numberOfClasses = 0;
	UInt32     dataSize;
	Boolean    isWritable;
	NSString*	    factoryClassName = 0;
	NSURL*	            CocoaViewBundlePath;

	OSStatus result = AudioUnitGetPropertyInfo (*au->get_au(),
						    kAudioUnitProperty_CocoaUI,
						    kAudioUnitScope_Global, 
						    0,
						    &dataSize,
						    &isWritable );

	numberOfClasses = (dataSize - sizeof(CFURLRef)) / sizeof(CFStringRef);
	
	// Does view have custom Cocoa UI?
	
	if ((result == noErr) && (numberOfClasses > 0) ) {
		cocoaViewInfo = (AudioUnitCocoaViewInfo *)malloc(dataSize);
		if(AudioUnitGetProperty(*au->get_au(),
					kAudioUnitProperty_CocoaUI,
					kAudioUnitScope_Global,
					0,
					cocoaViewInfo,
					&dataSize) == noErr) {

			CocoaViewBundlePath	= (NSURL *)cocoaViewInfo->mCocoaAUViewBundleLocation;
			
			// we only take the first view in this example.
			factoryClassName	= (NSString *)cocoaViewInfo->mCocoaAUViewClass[0];

		} else {

			if (cocoaViewInfo != NULL) {
				free (cocoaViewInfo);
				cocoaViewInfo = NULL;
			}
		}
	}

	NSRect crect = { { 0, 0 }, { 1, 1} };

	// [A] Show custom UI if view has it

	if (CocoaViewBundlePath && factoryClassName) {
		NSBundle *viewBundle  	= [NSBundle bundleWithPath:[CocoaViewBundlePath path]];
		if (viewBundle == nil) {
			error << _("AUPluginUI: error loading AU view's bundle") << endmsg;
			return -1;
		} else {
			Class factoryClass = [viewBundle classNamed:factoryClassName];
			if (!factoryClass) {
				error << _("AUPluginUI: error getting AU view's factory class from bundle") << endmsg;
				return -1;
			}
			
			// make sure 'factoryClass' implements the AUCocoaUIBase protocol
			if (!plugin_class_valid (factoryClass)) {
				error << _("AUPluginUI: U view's factory class does not properly implement the AUCocoaUIBase protocol") << endmsg;
				return -1;
			}
			// make a factory
			id factoryInstance = [[[factoryClass alloc] init] autorelease];
			if (factoryInstance == nil) {
				error << _("AUPluginUI: Could not create an instance of the AU view factory") << endmsg;
				return -1;
			}

			// make a view
			au_view = [factoryInstance uiViewForAudioUnit:*au->get_au() withSize:crect.size];
			
			// cleanup
			[CocoaViewBundlePath release];
			if (cocoaViewInfo) {
				UInt32 i;
				for (i = 0; i < numberOfClasses; i++)
					CFRelease(cocoaViewInfo->mCocoaAUViewClass[i]);
				
				free (cocoaViewInfo);
			}
			wasAbleToLoadCustomView = YES;
		}
	}

	if (!wasAbleToLoadCustomView) {
		// load generic Cocoa view
		au_view = [[AUGenericView alloc] initWithAudioUnit:*au->get_au()];
		[(AUGenericView *)au_view setShowsExpertParameters:YES];
	}

	return 0;
}

int
AUPluginUI::create_carbon_view ()
{
	OSStatus err;
	ControlRef root_control;

	Component editComponent = FindNextComponent(NULL, &carbon_descriptor);
	
	OpenAComponent(editComponent, &editView);
	if (!editView) {
		error << _("AU Carbon view: cannot open AU Component") << endmsg;
		return -1;
	}
	
	Rect r = { 100, 100, 100, 100 };
	WindowAttributes attr = WindowAttributes (kWindowStandardHandlerAttribute |
						  kWindowCompositingAttribute|
						  kWindowNoShadowAttribute|
						  kWindowNoTitleBarAttribute);

	if ((err = CreateNewWindow(kDocumentWindowClass, attr, &r, &carbon_window)) != noErr) {
		error << string_compose (_("AUPluginUI: cannot create carbon window (err: %1)"), err) << endmsg;
		return -1;
	}
	
	if ((err = GetRootControl(carbon_window, &root_control)) != noErr) {
		error << string_compose (_("AUPlugin: cannot get root control of carbon window (err: %1)"), err) << endmsg;
		return -1;
	}

	ControlRef viewPane;
	Float32Point location  = { 0.0, 0.0 };
	Float32Point size = { 0.0, 0.0 } ;

	if ((err = AudioUnitCarbonViewCreate (editView, *au->get_au(), carbon_window, root_control, &location, &size, &viewPane)) != noErr) {
		error << string_compose (_("AUPluginUI: cannot create carbon plugin view (err: %1)"), err) << endmsg;
		return -1;
	}

	// resize window

	Rect bounds;
	GetControlBounds(viewPane, &bounds);
	size.x = bounds.right-bounds.left;
	size.y = bounds.bottom-bounds.top;

	prefwidth = (int) (size.x + 0.5);
	prefheight = (int) (size.y + 0.5);

	SizeWindow (carbon_window, prefwidth, prefheight,  true);
	low_box.set_size_request (prefwidth, prefheight);

	return 0;
}

NSWindow*
AUPluginUI::get_nswindow ()
{
	Gtk::Container* toplevel = get_toplevel();

	if (!toplevel || !toplevel->is_toplevel()) {
		error << _("AUPluginUI: no top level window!") << endmsg;
		return 0;
	}

	NSWindow* true_parent = gdk_quartz_window_get_nswindow (toplevel->get_window()->gobj());

	if (!true_parent) {
		error << _("AUPluginUI: no top level window!") << endmsg;
		return 0;
	}

	return true_parent;
}

void
AUPluginUI::activate ()
{
	cerr << "AUPluginUI:: activate!\n";
	return;
	if (carbon_window && cocoa_parent) {
		cerr << "APP activated, activate carbon window " << insert->name() << endl;
		_activating_from_app = true;
		ActivateWindow (carbon_window, TRUE);
		_activating_from_app = false;
		[cocoa_parent makeKeyAndOrderFront:nil];
	} 
}

void
AUPluginUI::deactivate ()
{
	return;
	cerr << "APP DEactivated, for " << insert->name() << endl;
	_activating_from_app = true;
	ActivateWindow (carbon_window, FALSE);
	_activating_from_app = false;
}


OSStatus 
_carbon_event (EventHandlerCallRef nextHandlerRef, EventRef event, void *userData) 
{
	return ((AUPluginUI*)userData)->carbon_event (nextHandlerRef, event);
}

OSStatus 
AUPluginUI::carbon_event (EventHandlerCallRef nextHandlerRef, EventRef event)
{
	cerr << "CARBON EVENT\n";

	UInt32 eventKind = GetEventKind(event);
	ClickActivationResult howToHandleClick;
	NSWindow* win = get_nswindow ();

	cerr << "window " << win << " carbon event type " << eventKind << endl;

	switch (eventKind) {
	case kEventWindowHandleActivate:
		cerr << "carbon window for " << insert->name() << " activated\n";
		if (_activating_from_app) {
			cerr << "app activation, ignore window activation\n";
			return noErr;
		}
		[win makeMainWindow];
		return eventNotHandledErr;
		break;

	case kEventWindowHandleDeactivate:
		cerr << "carbon window for " << insert->name() << " deactivated\n";
		// never deactivate the carbon window
		return noErr;
		break;
		
	case kEventWindowGetClickActivation:
		cerr << "carbon window CLICK activated\n";
		[win makeKeyAndOrderFront:nil];
		howToHandleClick = kActivateAndHandleClick;
		SetEventParameter(event, kEventParamClickActivation, typeClickActivationResult, 
				  sizeof(ClickActivationResult), &howToHandleClick);
		break;
	}

	return noErr;
}

int
AUPluginUI::parent_carbon_window ()
{
	NSWindow* win = get_nswindow ();
	int x, y;

	if (!win) {
		return -1;
	}

	Gtk::Container* toplevel = get_toplevel();

	if (!toplevel || !toplevel->is_toplevel()) {
		error << _("AUPluginUI: no top level window!") << endmsg;
		return -1;
	}
	
	toplevel->get_window()->get_root_origin (x, y);

	/* compute how tall the title bar is, because we have to offset the position of the carbon window
	   by that much.
	*/

	NSRect content_frame = [NSWindow contentRectForFrameRect:[win frame] styleMask:[win styleMask]];
	NSRect wm_frame = [NSWindow frameRectForContentRect:content_frame styleMask:[win styleMask]];

	int titlebar_height = wm_frame.size.height - content_frame.size.height;

	int packing_extra = 6; // this is the total vertical packing in our top level window

	MoveWindow (carbon_window, x, y + titlebar_height + top_box.get_height() + packing_extra, false);
	ShowWindow (carbon_window);

	// create the cocoa window for the carbon one and make it visible
	cocoa_parent = [[NSWindow alloc] initWithWindowRef: carbon_window];

	EventTypeSpec	windowEventTypes[] = {
		{kEventClassWindow, kEventWindowGetClickActivation },
		{kEventClassWindow, kEventWindowHandleDeactivate }
	};
	
	EventHandlerUPP   ehUPP = NewEventHandlerUPP(_carbon_event);
	OSStatus result = InstallWindowEventHandler (carbon_window, ehUPP, 
						     sizeof(windowEventTypes) / sizeof(EventTypeSpec), 
						     windowEventTypes, this, &carbon_event_handler);
	if (result != noErr) {
		return -1;
	}

	[win addChildWindow:cocoa_parent ordered:NSWindowAbove];

	return 0;
}	

int
AUPluginUI::parent_cocoa_window ()
{
	NSWindow* win = get_nswindow ();
	NSView* packView = 0;
	NSRect packFrame;

	if (!win) {
		return -1;
	}

	Gtk::Container* toplevel = get_toplevel();

	if (!toplevel || !toplevel->is_toplevel()) {
		error << _("AUPluginUI: no top level window!") << endmsg;
		return -1;
	}
	
	// Get the size of the new AU View's frame 
	packFrame = [au_view frame];
	packFrame.origin.x = 0;
	packFrame.origin.y = 0;

	if (packFrame.size.width > 500 || packFrame.size.height > 500) {
		
		/* its too big - use a scrollview */

		NSRect frameRect = [[cocoa_window contentView] frame];
		scroll_view = [[[NSScrollView alloc] initWithFrame:frameRect] autorelease];
		[scroll_view setDrawsBackground:NO];
		[scroll_view setHasHorizontalScroller:YES];
		[scroll_view setHasVerticalScroller:YES];

		packFrame.size = [NSScrollView  frameSizeForContentSize:packFrame.size
				    hasHorizontalScroller:[scroll_view hasHorizontalScroller]
				    hasVerticalScroller:[scroll_view hasVerticalScroller]
				    borderType:[scroll_view borderType]];
		
		// Create a new frame with same origin as current
		// frame but size equal to the size of the new view
		NSRect newFrame;
		newFrame.origin = [scroll_view frame].origin;
		newFrame.size = packFrame.size;
		
		// Set the new frame and document views on the scroll view
		[scroll_view setFrame:newFrame];
		[scroll_view setDocumentView:au_view];
		
		packView = scroll_view;

	} else {

		packView = au_view;
	}

	NSView* view = gdk_quartz_window_get_nsview (low_box.get_window()->gobj());
	
	[view setFrame:packFrame];
	[view addSubview:packView]; 

	low_box.set_size_request (packFrame.size.width, packFrame.size.height);

	return 0;
}

void
AUPluginUI::on_realize ()
{
	VBox::on_realize ();

	/* our windows should not have that resize indicator */

	NSWindow* win = get_nswindow ();
	if (win) {
		[win setShowsResizeIndicator:NO];
	}
}

void
AUPluginUI::lower_box_realized ()
{
	if (au_view) {
		parent_cocoa_window ();
	} else if (carbon_window) {
		parent_carbon_window ();
	}
}

void
AUPluginUI::on_hide ()
{
	// VBox::on_hide ();
	cerr << "AU plugin window hidden\n";
}

bool
AUPluginUI::on_map_event (GdkEventAny* ev)
{
	cerr << "AU plugin map event\n";

	if (carbon_window) {

		// move top level GTK window to the correct level
		// to keep the stack together and not be sliceable
		
		NSWindow* win = get_nswindow ();
		// [win setLevel:NSFloatingWindowLevel];
	}

	return false;
}

void
AUPluginUI::on_show ()
{
	cerr << "AU plugin window shown\n";

	VBox::on_show ();

	gtk_widget_realize (GTK_WIDGET(low_box.gobj()));

	if (au_view) {
		show_all ();
	} else if (carbon_window) {
		[cocoa_parent setIsVisible:YES];
		ShowWindow (carbon_window);
	}
}

bool
AUPluginUI::start_updating (GdkEventAny* any)
{
	return false;
}

bool
AUPluginUI::stop_updating (GdkEventAny* any)
{
	return false;
}

PlugUIBase*
create_au_gui (boost::shared_ptr<PluginInsert> plugin_insert, VBox** box)
{
	AUPluginUI* aup = new AUPluginUI (plugin_insert);
	(*box) = aup;
	return aup;
}

bool
AUPluginUI::on_focus_in_event (GdkEventFocus* ev)
{
	cerr << "au plugin focus in\n";
	return false;
}

bool
AUPluginUI::on_focus_out_event (GdkEventFocus* ev)
{
	cerr << "au plugin focus out\n";
	return false;
}

