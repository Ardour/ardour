#include <gtkmm/stock.h>

#undef  Marker
#define Marker FuckYouAppleAndYourLackOfNameSpaces

#include "pbd/convert.h"
#include "pbd/error.h"

#include "ardour/audio_unit.h"
#include "ardour/debug.h"
#include "ardour/plugin_insert.h"

#undef check // stupid gtk, stupid apple

#include <gtkmm/button.h>
#include <gdk/gdkquartz.h>

#include <gtkmm2ext/utils.h>

#include "au_pluginui.h"
#include "gui_thread.h"

#include "appleutility/CAAudioUnit.h"
#include "appleutility/CAComponent.h"

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
		top_level_parent = tlp;
                
                if (cp) {
                        cocoa_parent = cp;
                        
                        [[NSNotificationCenter defaultCenter] addObserver:self
                         selector:@selector(cocoaParentActivationHandler:)
                         name:NSWindowDidBecomeMainNotification
                         object:nil];
                        
                        [[NSNotificationCenter defaultCenter] addObserver:self
                         selector:@selector(cocoaParentBecameKeyHandler:)
                         name:NSWindowDidBecomeKeyNotification
                         object:nil];
                }
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
        (void) notification; // stop complaints about unusued argument
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
	smaller_hbox->pack_start (_preset_combo, false, false);
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
	_preset_combo.show ();
	automation_mode_label.show ();
	automation_mode_selector.show ();
	bypass_button.show ();
	top_box.show ();
	low_box.show ();

	_activating_from_app = false;
	cocoa_parent = 0;
	_notify = 0;
	cocoa_window = 0;
	carbon_window = 0;
	au_view = 0;
	editView = 0;

	/* prefer cocoa, fall back to cocoa, but use carbon if its there */

	if (test_cocoa_view_support()) {
		create_cocoa_view ();
#ifdef WITH_CARBON
	} else if (test_carbon_view_support()) {
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
#endif

	if (editView) {
		CloseComponent (editView);
	}

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
	NSURL*	            CocoaViewBundlePath = NULL;

	OSStatus result = AudioUnitGetPropertyInfo (*au->get_au(),
						    kAudioUnitProperty_CocoaUI,
						    kAudioUnitScope_Global, 
						    0,
						    &dataSize,
						    &isWritable );

	numberOfClasses = (dataSize - sizeof(CFURLRef)) / sizeof(CFStringRef);

	// Does view have custom Cocoa UI?
	
	if ((result == noErr) && (numberOfClasses > 0) ) {

		DEBUG_TRACE(DEBUG::AudioUnits,
			    string_compose ( "based on %1, there are %2 cocoa UI classes\n", dataSize, numberOfClasses));

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
			
			DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("the factory name is %1 bundle is %2\n",
									[factoryClassName UTF8String], CocoaViewBundlePath));
                        
		} else {

			DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("No cocoaUI property cocoaViewInfo = %1\n", cocoaViewInfo));

			if (cocoaViewInfo != NULL) {
				free (cocoaViewInfo);
				cocoaViewInfo = NULL;
			}
		}
	}

	// [A] Show custom UI if view has it

	if (CocoaViewBundlePath && factoryClassName) {
		NSBundle *viewBundle  	= [NSBundle bundleWithPath:[CocoaViewBundlePath path]];

		DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("tried to create bundle, result = %1\n", viewBundle));

		if (viewBundle == nil) {
			error << _("AUPluginUI: error loading AU view's bundle") << endmsg;
			return -1;
		} else {
			Class factoryClass = [viewBundle classNamed:factoryClassName];
			DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("tried to create factory class, result = %1\n", factoryClass));
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
			id factory = [[[factoryClass alloc] init] autorelease];
			if (factory == nil) {
				error << _("AUPluginUI: Could not create an instance of the AU view factory") << endmsg;
				return -1;
			}

			DEBUG_TRACE (DEBUG::AudioUnits, "got a factory instance\n");

			// make a view
			au_view = [factory uiViewForAudioUnit:*au->get_au() withSize:NSZeroSize];

			DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("view created @ %1\n", au_view));
			
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
		DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("Loading generic view using %1 -> %2\n", au,
								au->get_au()));
		au_view = [[AUGenericView alloc] initWithAudioUnit:*au->get_au()];
		DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("view created @ %1\n", au_view));
		[(AUGenericView *)au_view setShowsExpertParameters:YES];
	}

	// Get the initial size of the new AU View's frame 
	
	NSRect rect = [au_view frame];
	low_box.set_size_request (rect.size.width, rect.size.height);
	
	return 0;
}

void
AUPluginUI::cocoa_view_resized ()
{
        GtkRequisition topsize = top_box.size_request ();
        NSWindow* window = get_nswindow ();
        NSSize oldContentSize= [window contentRectForFrameRect:[window frame]].size;
        NSSize newContentSize= [au_view frame].size;
        NSRect windowFrame= [window frame];
        
        oldContentSize.height -= topsize.height;

        float dy = oldContentSize.height - newContentSize.height;
        float dx = oldContentSize.width - newContentSize.width;

        windowFrame.origin.y    += dy;
        windowFrame.origin.x    += dx;
        windowFrame.size.height -= dy;
        windowFrame.size.width  -= dx;
        
        [[NSNotificationCenter defaultCenter] removeObserver:_notify
         name:NSViewFrameDidChangeNotification 
         object:au_view];

        NSUInteger old_auto_resize = [au_view autoresizingMask];

        [au_view setAutoresizingMask:NSViewNotSizable];
        [window setFrame:windowFrame display:YES];
        [au_view setAutoresizingMask:old_auto_resize];

        [[NSNotificationCenter defaultCenter] addObserver:_notify
         selector:@selector(auViewResized:) name:NSViewFrameDidChangeNotification
         object:au_view];
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
#ifdef WITH_CARBON
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
	[view addSubview:au_view positioned:NSWindowBelow relativeTo:nil]; 

	// watch for size changes of the view

	_notify = [ [NotificationObject alloc] initWithPluginUI:this andCocoaParent:nil andTopLevelParent:win ]; 

        [[NSNotificationCenter defaultCenter] addObserver:_notify
         selector:@selector(auViewResized:) name:NSViewFrameDidChangeNotification
         object:au_view];

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
	} else if (carbon_window) {
		parent_carbon_window ();
	}
}

bool
AUPluginUI::on_map_event (GdkEventAny*)
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
AUPluginUI::on_window_show (const string& /*title*/)
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
AUPluginUI::start_updating (GdkEventAny*)
{
	return false;
}

bool
AUPluginUI::stop_updating (GdkEventAny*)
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
AUPluginUI::on_focus_in_event (GdkEventFocus*)
{
	//cerr << "au plugin focus in\n";
	//Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
AUPluginUI::on_focus_out_event (GdkEventFocus*)
{
	//cerr << "au plugin focus out\n";
	//Keyboard::magic_widget_drop_focus ();
	return false;
}

