#undef  Marker
#define Marker FuckYouAppleAndYourLackOfNameSpaces

#include <gtkmm/button.h>
#include <gdk/gdkquartz.h>

#include "pbd/convert.h"
#include "pbd/error.h"

#include "ardour/audio_unit.h"
#include "ardour/debug.h"
#include "ardour/plugin_insert.h"

#undef check // stupid gtk, stupid apple

#include <gtkmm2ext/utils.h>

#include "au_pluginui.h"
#include "gui_thread.h"

#include "CAAudioUnit.h"
#include "CAComponent.h"

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

static void
dump_view_tree (NSView* view, int depth, int maxdepth)
{
	NSArray* subviews = [view subviews];
	unsigned long cnt = [subviews count];

	for (int d = 0; d < depth; d++) {
		cerr << '\t';
	}
	NSRect frame = [view frame];
	cerr << " view @ " <<  frame.origin.x << ", " << frame.origin.y
		<< ' ' << frame.size.width << " x " << frame.size.height
		<< endl;

	if (depth >= maxdepth) {
		return;
	}
	for (unsigned long i = 0; i < cnt; ++i) {
		NSView* subview = [subviews objectAtIndex:i];
		dump_view_tree (subview, depth+1, maxdepth);
	}
}

@implementation NotificationObject

- (NotificationObject*) initWithPluginUI: (AUPluginUI*) apluginui andCocoaParent: (NSWindow*) cp andTopLevelParent: (NSWindow*) tlp
{
	self = [ super init ];

	if (self) {
		plugin_ui = apluginui;
		top_level_parent = tlp;

		if (cp) {
			cocoa_parent = cp;

			[[NSNotificationCenter defaultCenter]
			     addObserver:self
			        selector:@selector(cocoaParentActivationHandler:)
			            name:NSWindowDidBecomeMainNotification
			          object:NULL];

			[[NSNotificationCenter defaultCenter]
			     addObserver:self
			        selector:@selector(cocoaParentBecameKeyHandler:)
			            name:NSWindowDidBecomeKeyNotification
			          object:NULL];
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

- (void)auViewResized:(NSNotification *)notification
{
	(void) notification; // stop complaints about unusued argument
	plugin_ui->cocoa_view_resized();
}

@end

AUPluginUI::AUPluginUI (boost::shared_ptr<PluginInsert> insert)
	: PlugUIBase (insert)
	, automation_mode_label (_("Automation"))
	, preset_label (_("Presets"))
	, mapped (false)
	, resizable (false)
	, min_width (0)
	, min_height (0)
	, req_width (0)
	, req_height (0)
	, alo_width (0)
	, alo_height (0)

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
	smaller_hbox->pack_start (_preset_modified, false, false);
	smaller_hbox->pack_start (_preset_combo, false, false);
	smaller_hbox->pack_start (add_button, false, false);
#if 0
	/* Ardour does not currently allow to overwrite existing presets
	 * see save_property_list() in audio_unit.cc
	 */
	smaller_hbox->pack_start (save_button, false, false);
#endif
#if 0
	/* one day these might be useful with an AU plugin, but not yet */
	smaller_hbox->pack_start (automation_mode_label, false, false);
	smaller_hbox->pack_start (automation_mode_selector, false, false);
#endif
	smaller_hbox->pack_start (reset_button, false, false);
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
	pack_start (low_box, true, true);

	preset_label.show ();
	_preset_combo.show ();
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
	carbon_window = 0;
#endif

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

	low_box.add_events (Gdk::VISIBILITY_NOTIFY_MASK | Gdk::EXPOSURE_MASK);

	low_box.signal_realize().connect (mem_fun (this, &AUPluginUI::lower_box_realized));
	low_box.signal_visibility_notify_event ().connect (mem_fun (this, &AUPluginUI::lower_box_visibility_notify));
	low_box.signal_size_request ().connect (mem_fun (this, &AUPluginUI::lower_box_size_request));
	low_box.signal_size_allocate ().connect (mem_fun (this, &AUPluginUI::lower_box_size_allocate));
	low_box.signal_map ().connect (mem_fun (this, &AUPluginUI::lower_box_map));
	low_box.signal_unmap ().connect (mem_fun (this, &AUPluginUI::lower_box_unmap));
	//low_box.signal_expose_event ().connect (mem_fun (this, &AUPluginUI::lower_box_expose));
}

AUPluginUI::~AUPluginUI ()
{
	if (_notify) {
		[[NSNotificationCenter defaultCenter] removeObserver:_notify];
	}

	if (cocoa_parent) {
		NSWindow* win = get_nswindow();
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
	bool wasAbleToLoadCustomView = false;
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

		if (viewBundle == NULL) {
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
			if (factory == NULL) {
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
			wasAbleToLoadCustomView = true;
		}
	}

	if (!wasAbleToLoadCustomView) {
		// load generic Cocoa view
		DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("Loading generic view using %1 -> %2\n", au,
								au->get_au()));
		au_view = [[AUGenericView alloc] initWithAudioUnit:*au->get_au()];
		DEBUG_TRACE (DEBUG::AudioUnits, string_compose ("view created @ %1\n", au_view));
		[(AUGenericView *)au_view setShowsExpertParameters:1];
	}

	// Get the initial size of the new AU View's frame
	NSRect  frame = [au_view frame];
	min_width  = req_width  = CGRectGetWidth(NSRectToCGRect(frame));
	min_height = req_height = CGRectGetHeight(NSRectToCGRect(frame));
	resizable  = [au_view autoresizingMask];

	low_box.queue_resize ();

	return 0;
}

void
AUPluginUI::cocoa_view_resized ()
{
	if (!mapped || alo_width == 0 || alo_height == 0 || !resizable) {
		return;
	}
	/* check for self-resizing plugins (e.g expand settings in AUSampler)
	 * if the widget expands it moves its y-offset (cocoa y-axis points towards the top)
	 */
	NSRect new_au_frame = [au_view frame];

	//float dx = last_au_frame.origin.x - new_au_frame.origin.x;
	float dy = last_au_frame.origin.y - new_au_frame.origin.y;
	//req_width += dx;
	req_height += dy;
	if (req_width < min_width) req_width = min_width;
	if (req_height < min_height) req_height = min_height;

	last_au_frame = new_au_frame;
	low_box.queue_resize ();
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

	if ((err = CreateNewWindow(kUtilityWindowClass, attr, &r, &carbon_window)) != noErr) {
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

	req_width = (int) (size.x + 0.5);
	req_height = (int) (size.y + 0.5);

	SizeWindow (carbon_window, prefwidth, req_height,  true);
	low_box.set_size_request (prefwidth, req_height); // ??

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
	Rect windowStructureBoundsRect;

	if (!win) {
		return -1;
	}

	Gtk::Container* toplevel = get_toplevel();

	if (!toplevel || !toplevel->is_toplevel()) {
		error << _("AUPluginUI: no top level window!") << endmsg;
		return -1;
	}

	/* figure out where the cocoa parent window is in carbon-coordinate space, which
	   differs from both cocoa-coordinate space and GTK-coordinate space
	*/

	GetWindowBounds((WindowRef) [win windowRef], kWindowStructureRgn, &windowStructureBoundsRect);

	/* compute how tall the title bar is, because we have to offset the position of the carbon window
	   by that much.
	*/

	NSRect content_frame = [NSWindow contentRectForFrameRect:[win frame] styleMask:[win styleMask]];
	NSRect wm_frame = [NSWindow frameRectForContentRect:content_frame styleMask:[win styleMask]];

	int titlebar_height = wm_frame.size.height - content_frame.size.height;

	int packing_extra = 6; // this is the total vertical packing in our top level window

	/* move into position, based on parent window position */
	MoveWindow (carbon_window,
		    windowStructureBoundsRect.left,
		    windowStructureBoundsRect.top + titlebar_height + top_box.get_height() + packing_extra,
		    false);
	ShowWindow (carbon_window);

	// create the cocoa window for the carbon one and make it visible
	cocoa_parent = [[NSWindow alloc] initWithWindowRef: carbon_window];

	SetWindowActivationScope (carbon_window, kWindowActivationScopeNone);

	_notify = [ [NotificationObject alloc] initWithPluginUI:this andCocoaParent:cocoa_parent andTopLevelParent:win ];

	[win addChildWindow:cocoa_parent ordered:NSWindowAbove];
	[win setAutodisplay:1]; // turn of GTK stuff for this window

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

	//[win setAutodisplay:1]; // turn off GTK stuff for this window

	NSView* view = gdk_quartz_window_get_nsview (low_box.get_window()->gobj());
	[view addSubview:au_view];

	gint xx, yy;
	gtk_widget_translate_coordinates(
			GTK_WIDGET(low_box.gobj()),
			GTK_WIDGET(low_box.get_parent()->gobj()),
			8, 6, &xx, &yy);
	[au_view setFrame:NSMakeRect(xx, yy, req_width, req_height)];

	last_au_frame = [au_view frame];
	// watch for size changes of the view
	_notify = [ [NotificationObject alloc] initWithPluginUI:this andCocoaParent:NULL andTopLevelParent:win ];

	[[NSNotificationCenter defaultCenter] addObserver:_notify
		selector:@selector(auViewResized:) name:NSViewFrameDidChangeNotification
		object:au_view];

	return 0;
}

void
AUPluginUI::grab_focus()
{
	if (au_view) {
		[au_view becomeFirstResponder];
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
		[win setShowsResizeIndicator:0];
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
AUPluginUI::lower_box_visibility_notify (GdkEventVisibility* ev)
{
#ifdef WITH_CARBON
	if (carbon_window  && ev->state != GDK_VISIBILITY_UNOBSCURED) {
		ShowWindow (carbon_window);
		ActivateWindow (carbon_window, TRUE);
		return true;
	}
#endif
	return false;
}

void
AUPluginUI::update_view_size ()
{
	if (!mapped || alo_width == 0 || alo_height == 0) {
		return;
	}
	gint xx, yy;
	gtk_widget_translate_coordinates(
			GTK_WIDGET(low_box.gobj()),
			GTK_WIDGET(low_box.get_parent()->gobj()),
			8, 6, &xx, &yy);

	[[NSNotificationCenter defaultCenter] removeObserver:_notify
		name:NSViewFrameDidChangeNotification
		object:au_view];

	if (!resizable) {
		xx += (alo_width - req_width) * .5;
		[au_view setFrame:NSMakeRect(xx, yy, req_width, req_height)];
	} else {
		/* this mitigates issues with plugins that resize themselves
		 * depending on visible options (e.g AUSampler)
		 * since the OSX y-axis points upwards, the plugin adjusts its
		 * own y-offset if the view expands to the bottom to accomodate
		 * subviews inside the main view.
		 */
		[au_view setAutoresizesSubviews:0];
		[au_view setFrame:NSMakeRect(xx, yy, alo_width, alo_height)];
		[au_view setAutoresizesSubviews:1];
		[au_view setNeedsDisplay:1];
	}

	last_au_frame = [au_view frame];

	[[NSNotificationCenter defaultCenter]
	     addObserver:_notify
	        selector:@selector(auViewResized:) name:NSViewFrameDidChangeNotification
	          object:au_view];
}

void
AUPluginUI::lower_box_map ()
{
	mapped = true;
	[au_view setHidden:0];
	update_view_size ();
}

void
AUPluginUI::lower_box_unmap ()
{
	mapped = false;
	[au_view setHidden:1];
}

void
AUPluginUI::lower_box_size_request (GtkRequisition* requisition)
{
	requisition->width  = req_width;
	requisition->height = req_height;
}

void
AUPluginUI::lower_box_size_allocate (Gtk::Allocation& allocation)
{
	alo_width  = allocation.get_width ();
	alo_height = allocation.get_height ();
	update_view_size ();
}

gboolean
AUPluginUI::lower_box_expose (GdkEventExpose* event)
{
	[au_view drawRect:NSMakeRect(event->area.x,
			event->area.y,
			event->area.width,
			event->area.height)];
	return true;
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

#if 0
	NSArray* wins = [NSApp windows];
	for (uint32_t i = 0; i < [wins count]; i++) {
		id win = [wins objectAtIndex:i];
	}
#endif
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


