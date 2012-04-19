#undef  Marker
#define Marker FuckYouAppleAndYourLackOfNameSpaces

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

#undef Marker

#include "keyboard.h"
#include "utils.h"
#include "public_editor.h"
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

@implementation NotificationObject

- (NotificationObject*) initWithPluginUI: (AUPluginUI*) apluginui andCocoaParent: (NSWindow*) cp andTopLevelParent: (NSWindow*) tlp
{
	self = [ super init ];

	if (self) {
		plugin_ui = apluginui;
		cocoa_parent = cp;
		top_level_parent = tlp;

		[[NSNotificationCenter defaultCenter] addObserver:self
		 selector:@selector(cocoaParentActivationHandler:)
		 name:NSWindowDidBecomeMainNotification
		 object:nil];

		[[NSNotificationCenter defaultCenter] addObserver:self
		 selector:@selector(cocoaParentBecameKeyHandler:)
		 name:NSWindowDidBecomeKeyNotification
		 object:nil];
	}

	return self;
}
		
- (void)cocoaParentActivationHandler:(NSNotification *)notification
{
	NSWindow* notification_window = (NSWindow *)[notification object];

	if (top_level_parent == notification_window || cocoa_parent == notification_window) {
		if ([notification_window isMainWindow]) {
			plugin_ui->activate();
		} else {
			plugin_ui->deactivate();
		}
	} 
}

- (void)cocoaParentBecameKeyHandler:(NSNotification *)notification
{
	NSWindow* notification_window = (NSWindow *)[notification object];

	if (top_level_parent == notification_window || cocoa_parent == notification_window) {
		if ([notification_window isKeyWindow]) {
			plugin_ui->activate();
		} else {
			plugin_ui->deactivate();
		}
	} 
}

- (void)auViewResized:(NSNotification *)notification;
{
	plugin_ui->cocoa_view_resized();
}

@end

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

	HBox* smaller_hbox = manage (new HBox);

	smaller_hbox->set_spacing (6);
	smaller_hbox->pack_start (preset_label, false, false, 4);
	smaller_hbox->pack_start (preset_combo, false, false);
	smaller_hbox->pack_start (save_button, false, false);
#if 0
	/* one day these might be useful with an AU plugin, but not yet */
	smaller_hbox->pack_start (automation_mode_label, false, false);
	smaller_hbox->pack_start (automation_mode_selector, false, false);
#endif
	smaller_hbox->pack_start (bypass_button, false, true);

	VBox* v1_box = manage (new VBox);
	VBox* v2_box = manage (new VBox);

	v1_box->pack_start (*smaller_hbox, false, true);
	v2_box->pack_start (focus_button, false, true);

	top_box.set_homogeneous (false);
	top_box.set_spacing (6);
	top_box.set_border_width (6);

	top_box.pack_end (*v2_box, false, false);
	top_box.pack_end (*v1_box, false, false);

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

	cocoa_parent = 0;
	cocoa_window = 0;

#ifdef WITH_CARBON
	_activating_from_app = false;
	_notify = 0;
	au_view = 0;
	editView = 0;
#endif

	/* prefer cocoa, fall back to cocoa, but use carbon if its there */

	if (test_cocoa_view_support()) {
	        cerr << insert->name() << " creating cocoa view\n";
		if (create_cocoa_view () != 0) {
#ifdef WITH_CARBON
			if (test_carbon_view_support()) {
				cerr << insert->name() << " falling back to carbon view\n";
				create_carbon_view ();
			}
#endif
		}
#ifdef WITH_CARBON
	} else if (test_carbon_view_support()) {
	        cerr << insert->name() << " creating carbon view\n";
		create_carbon_view ();
#endif
	} else {
		create_cocoa_view ();
	}

	low_box.signal_realize().connect (mem_fun (this, &AUPluginUI::lower_box_realized));
}

AUPluginUI::~AUPluginUI ()
{
	if (cocoa_parent) {
		NSWindow* win = get_nswindow();
		[[NSNotificationCenter defaultCenter] removeObserver:_notify];
		[win removeChildWindow:cocoa_parent];

	} 

#ifdef WITH_CARBON
	if (carbon_window) {
		/* not parented, just overlaid on top of our window */
		DisposeWindow (carbon_window);
	}

	if (editView) {
		CloseComponent (editView);
	}
#endif

	if (au_view) {
		/* remove whatever we packed into low_box so that GTK doesn't
		   mess with it.
		*/

		[au_view removeFromSuperview];
	}
}

bool
AUPluginUI::test_carbon_view_support ()
{
#ifdef WITH_CARBON
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
#else
	return false;
#endif
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
	UInt32     numberOfClasses = 0;
	UInt32     dataSize;
	Boolean    isWritable;
	NSString*  factoryClassName = 0;
	NSURL*     CocoaViewBundlePath = 0;

	OSStatus result = AudioUnitGetPropertyInfo (*au->get_au(),
						    kAudioUnitProperty_CocoaUI,
						    kAudioUnitScope_Global, 
						    0,
						    &dataSize,
						    &isWritable );

	numberOfClasses = (dataSize - sizeof(CFURLRef)) / sizeof(CFStringRef);
	
	// Does view have custom Cocoa UI?
	
	if ((result == noErr) && (numberOfClasses > 0) ) {
		cerr << insert->name() << " has " << numberOfClasses << " cocoa UI classes\n";
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
			if (factoryClassName) {
				cerr << insert->name() << " no factory name class name provided\n";
			} else {
				const char* fcn = [factoryClassName UTF8String];
				cerr << insert->name() << " fetching cocoa UI via factory called " << fcn << '\n';
			}

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
				cerr << _("AUPluginUI: error getting AU view's factory class from bundle") << endl;
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

	// watch for size changes of the view

	 [[NSNotificationCenter defaultCenter] addObserver:_notify
	       selector:@selector(auViewResized:) name:NSWindowDidResizeNotification
	       object:au_view];

	// Get the size of the new AU View's frame 
	
	NSRect packFrame;
	packFrame = [au_view frame];
	prefwidth = packFrame.size.width;
	prefheight = packFrame.size.height;
	low_box.set_size_request (prefwidth, prefheight);
	
	return 0;
}

void
AUPluginUI::cocoa_view_resized ()
{
	NSRect packFrame = [au_view frame];
}

int
AUPluginUI::create_carbon_view ()
{
#ifdef WITH_CARBON
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
	        CloseComponent (editView);
		return -1;
	}
	
	if ((err = GetRootControl(carbon_window, &root_control)) != noErr) {
		error << string_compose (_("AUPlugin: cannot get root control of carbon window (err: %1)"), err) << endmsg;
		DisposeWindow (carbon_window);
	        CloseComponent (editView);
		return -1;
	}

	ControlRef viewPane;
	Float32Point location  = { 0.0, 0.0 };
	Float32Point size = { 0.0, 0.0 } ;

	if ((err = AudioUnitCarbonViewCreate (editView, *au->get_au(), carbon_window, root_control, &location, &size, &viewPane)) != noErr) {
		error << string_compose (_("AUPluginUI: cannot create carbon plugin view (err: %1)"), err) << endmsg;
		DisposeWindow (carbon_window);
	        CloseComponent (editView);
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
#else
	error << _("AU Carbon GUI is not supported.") << endmsg;
	return -1;
#endif
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
#ifdef WITN_CARBON
	ActivateWindow (carbon_window, TRUE);
#endif
	// [cocoa_parent makeKeyAndOrderFront:nil];
}

void
AUPluginUI::deactivate ()
{
#ifdef WITH_CARBON
	ActivateWindow (carbon_window, FALSE);
#endif
}

int
AUPluginUI::parent_carbon_window ()
{
#ifdef WITH_CARBON
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

	SetWindowActivationScope (carbon_window, kWindowActivationScopeNone);

	_notify = [ [NotificationObject alloc] initWithPluginUI:this andCocoaParent:cocoa_parent andTopLevelParent:win ]; 

	[win addChildWindow:cocoa_parent ordered:NSWindowAbove];

	return 0;
#else
	return -1;
#endif
}	

int
AUPluginUI::parent_cocoa_window ()
{
	NSWindow* win = get_nswindow ();

	if (!win) {
		return -1;
	}

	[win setAutodisplay:YES]; // turn of GTK stuff for this window

	Gtk::Container* toplevel = get_toplevel();

	if (!toplevel || !toplevel->is_toplevel()) {
		error << _("AUPluginUI: no top level window!") << endmsg;
		return -1;
	}

	NSView* view = gdk_quartz_window_get_nsview (get_toplevel()->get_window()->gobj());
	GtkRequisition a = top_box.size_request ();

	/* move the au_view down so that it doesn't overlap the top_box contents */

	NSPoint origin = { 0, a.height };

	[au_view setFrameOrigin:origin];
	[view addSubview:au_view]; 

	return 0;
}

static void
dump_view_tree (NSView* view, int depth)
{
	NSArray* subviews = [view subviews];
	unsigned long cnt = [subviews count];

	for (int d = 0; d < depth; d++) {
		cerr << '\t';
	}
	cerr << " view @ " << view << endl;
	
	for (unsigned long i = 0; i < cnt; ++i) {
		NSView* subview = [subviews objectAtIndex:i];
		dump_view_tree (subview, depth+1);
	}
}

void
AUPluginUI::forward_key_event (GdkEventKey* ev)
{
	NSEvent* nsevent = gdk_quartz_event_get_nsevent ((GdkEvent*)ev);

	if (au_view && nsevent) {

		/* filter on nsevent type here because GDK massages FlagsChanged
		   messages into GDK_KEY_{PRESS,RELEASE} but Cocoa won't
		   handle a FlagsChanged message as a keyDown or keyUp
		*/

		if ([nsevent type] == NSKeyDown) {
			[[[au_view window] firstResponder] keyDown:nsevent];
		} else if ([nsevent type] == NSKeyUp) {
			[[[au_view window] firstResponder] keyUp:nsevent];
		} else if ([nsevent type] == NSFlagsChanged) {
			[[[au_view window] firstResponder] flagsChanged:nsevent];
		}
	}
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
#ifdef WITH_CARBON
	} else if (carbon_window) {
		parent_carbon_window ();
#endif
	}
}

bool
AUPluginUI::on_map_event (GdkEventAny* ev)
{
	return false;
}

void
AUPluginUI::on_window_hide ()
{
#ifdef WITH_CARBON
	if (carbon_window) {
		HideWindow (carbon_window);
		ActivateWindow (carbon_window, FALSE);
	}
#endif

	hide_all ();
}

bool
AUPluginUI::on_window_show (const string& title)
{
	/* this is idempotent so just call it every time we show the window */

	gtk_widget_realize (GTK_WIDGET(low_box.gobj()));

	show_all ();

#ifdef WITH_CARBON
	if (carbon_window) {
		ShowWindow (carbon_window);
		ActivateWindow (carbon_window, TRUE);
	}
#endif

	return true;
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
	//cerr << "au plugin focus in\n";
	//Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
AUPluginUI::on_focus_out_event (GdkEventFocus* ev)
{
	//cerr << "au plugin focus out\n";
	//Keyboard::magic_widget_drop_focus ();
	return false;
}

