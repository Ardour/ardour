/*
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#undef  Marker
#define Marker FuckYouAppleAndYourLackOfNameSpaces

#include <sys/time.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gdk/gdkquartz.h>

#include "pbd/convert.h"
#include "pbd/error.h"

#include "ardour/auditioner.h"
#include "ardour/audio_unit.h"
#include "ardour/debug.h"
#include "ardour/plugin_insert.h"

#undef check // stupid gtk, stupid apple

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_proxy.h>

#include "au_pluginui.h"
#include "gui_thread.h"
#include "processor_box.h"

// yes, yes we know (see wscript for various available OSX compat modes)
#if defined (__clang__)
#	pragma clang diagnostic push
#	pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "CAAudioUnit.h"
#include "CAComponent.h"

#if defined (__clang__)
#	pragma clang diagnostic pop
#endif

#import <AudioUnit/AUCocoaUIView.h>
#import <CoreAudioKit/AUGenericView.h>
#import <objc/runtime.h>

#ifndef __ppc__
#include <dispatch/dispatch.h>
#endif

#undef Marker

#include "keyboard.h"
#include "utils.h"
#include "public_editor.h"
#include "pbd/i18n.h"

#include "gtk2ardour-config.h"

#ifdef COREAUDIO105
#define ArdourCloseComponent CloseComponent
#else
#define ArdourCloseComponent AudioComponentInstanceDispose
#endif
using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace PBD;

vector<string> AUPluginUI::automation_mode_strings;
int64_t AUPluginUI::last_timer = 0;
bool    AUPluginUI::timer_needed = true;
CFRunLoopTimerRef AUPluginUI::cf_timer;
sigc::connection AUPluginUI::timer_connection;

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

	if (depth == 0) {
		NSView* su = [view superview];
		if (su) {
			NSRect sf = [su frame];
			cerr << " PARENT view " << su << " @ " <<  sf.origin.x << ", " << sf.origin.y
			     << ' ' << sf.size.width << " x " << sf.size.height
			     << endl;
		}
	}

	for (int d = 0; d < depth; d++) {
		cerr << '\t';
	}
	NSRect frame = [view frame];
	cerr << " view " << view << " @ " <<  frame.origin.x << ", " << frame.origin.y
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

/* This deeply hacky block of code exists for a rather convoluted reason.
 *
 * The proximal reason is that there are plugins (such as XLN's Addictive Drums
 * 2) which redraw their GUI/editor windows using a timer, and use a drawing
 * technique that on Retina displays ends up calling arg32_image_mark_RGB32, a
 * function that for some reason (probably byte-swapping or pixel-doubling) is
 * many times slower than the function used on non-Retina displays.
 *
 * We are not the first people to discover the problem with
 * arg32_image_mark_RGB32.
 *
 * Justin Fraenkel, the lead author of Reaper, wrote a very detailed account of
 * the performance issues with arg32_image_mark_RGB32 here:
 * http://www.1014.org/?article=516
 *
 * The problem was also seen by Robert O'Callahan (lead developer of rr, the
 * reverse debugger) as far back as 2010:
 * http://robert.ocallahan.org/2010/05/cglayer-performance-trap-with-isflipped_03.html
 *
 * In fact, it is so slow that the drawing takes up close to 100% of a single
 * core, and the event loop that the drawing occurs in never sleeps or "idles".
 *
 * In AU hosts built directly on top of Cocoa, or some other toolkits, this
 * isn't inherently a major problem - it just makes the entire GUI of the
 * application slow.
 *
 * However, there is an additional problem for Ardour because GTK+ is built on
 * top of the GDK/Quartz event loop integration. This integration is rather
 * baroque, mostly because it was written at a time when CFRunLoop did not
 * offer a way to wait for "input" from file descriptors (which arrived in OS X
 * 10.5). As a result, it uses a hair-raising design involving an additional
 * thread. This design has a major problem, which is that it effectively
 * creates two nested run loops.
 *
 * The GTK+/GDK/glib one runs until it has nothing to do, at which time it
 * calls a function to wait until there is something to do. On Linux or Windows
 * that would involve some variant or relative of poll(2), which puts the
 * process to sleep until there is something to do.
 *
 * On OS X, glib ends up calling [CFRunLoop waitForNextEventMatchingMask] which
 * will eventually put the process to sleep, but won't do so until the
 * CFRunLoop also has nothing to do. This includes (at least) a complete redraw
 * cycle. If redrawing takes too long, and there are timers expired for another
 * redraw (e.g. Addictive Drums 2, again), then the CFRunLoop will just start
 * another redraw cycle after processing any events and other stuff.
 *
 * If the CFRunLoop stays busy, then it will never return to the glib
 * level at all, thus stopping any further GTK+ level activity (events,
 * drawing) from taking place. In short, the current (spring 2016) design of
 * the GDK/Quartz event loop integration relies on the idea that the internal
 * CFRunLoop will go idle, and totally breaks if this does not happen.
 *
 * So take a fully functional Ardour, add in XLN's Addictive Drums 2, and a
 * Retina display, and Apple's ridiculously slow blitting code, and the
 * CFRunLoop never goes idle. As soon as Addictive Drums starts drawing (over
 * and over again), the GTK+ event loop stops receiving events and stops
 * drawing.
 *
 * One fix for this was to run a nested GTK+ event loop iteration (or two)
 * whenever a plugin window was redrawn. This works in the sense that the
 * immediate issue (no GTK+ events or drawing) is fixed. But the recursive GTK+
 * event loop causes its own (very subtle) problems too.
 *
 * This code takes a rather radical approach. We use Objective C's ability to
 * swizzle object methods. Specifically, we replace [NSView displayIfNeeded]
 * with our own version which will skip redraws of plugin windows if we tell it
 * too. If we haven't done that, or if the redraw is of a non-plugin window,
 * then we invoke the original displayIfNeeded method.
 *
 * After every 10 redraws of a given plugin GUI/editor window, we queue up a
 * GTK/glib idle callback to measure the interval between those idle
 * callbacks. We do this globally across all plugin windows, so if the callback
 * is already queued, we don't requeue it.
 *
 * If the interval is longer than 40msec (a 25fps redraw rate), we set
 * block_plugin_redraws to some number. Each successive call to our interposed
 * displayIfNeeded method will (a) check this value and if non-zero (b) check
 * if the call is for a plugin-related NSView/NSWindow. If it is, then we will
 * skip the redisplay entirely, hopefully avoiding any calls to
 * argb32_image_mark_RGB32 or any other slow drawing code, and thus allowing
 * the CFRunLoop to go idle. If the value is zero or the call is for a
 * non-plugin window, then we just invoke the "original" displayIfNeeded
 * method.
 *
 * This hack adds a tiny bit of overhead onto redrawing of the entire
 * application. But in the common case this consists of 1 conditional (the
 * check on block_plugin_redraws, which will find it to be zero) and the
 * invocation of the original method. Given how much work is typically done
 * during drawing, this seems acceptable.
 *
 * The correct fix for this is to redesign the relationship between
 * GTK+/GDK/glib so that a glib run loop is actually a CFRunLoop, with all
 * GSources represented as CFRunLoopSources, without any nesting and without
 * any additional thread. This is not a task to be undertaken lightly, and is
 * certainly substantially more work than this was. It may never be possible to
 * do that work in a way that could be integrated back into glib, because of
 * the rather specific semantics and types of GSources, but it would almost
 * certainly be possible to make it work for Ardour.
 */

static uint32_t block_plugin_redraws = 0;
static const uint32_t minimum_redraw_rate = 30; /* frames per second */
static const uint32_t block_plugin_redraw_count = 15; /* number of combined plugin redraws to block, if blocking */

#ifdef __ppc__

/* PowerPC versions of OS X do not support libdispatch, which we use below when swizzling objective C. But they also don't have Retina
 * which is the underlying reason for this code. So just skip it on those CPUs.
 */


static void add_plugin_view (id view) {}
static void remove_plugin_view (id view) {}

#else

static IMP original_nsview_drawIfNeeded;
static std::vector<id> plugin_views;

static void add_plugin_view (id view)
{
	if (plugin_views.empty()) {
		AUPluginUI::start_cf_timer ();
	}

	plugin_views.push_back (view);

}

static void remove_plugin_view (id view)
{
	std::vector<id>::iterator x = find (plugin_views.begin(), plugin_views.end(), view);
	if (x != plugin_views.end()) {
		plugin_views.erase (x);
	}
	if (plugin_views.empty()) {
		AUPluginUI::stop_cf_timer ();
	}
}

static void interposed_drawIfNeeded (id receiver, SEL selector, NSRect rect)
{
	if (block_plugin_redraws && (find (plugin_views.begin(), plugin_views.end(), receiver) != plugin_views.end())) {
		block_plugin_redraws--;
#ifdef AU_DEBUG_PRINT
		std::cerr << "Plugin redraw blocked\n";
#endif
		/* YOU ... SHALL .... NOT ... DRAW!!!! */
		return;
	}
	(void) ((int (*)(id,SEL,NSRect)) original_nsview_drawIfNeeded) (receiver, selector, rect);
}

@implementation NSView (Tracking)
+ (void) load {
	static dispatch_once_t once_token;

	/* this swizzles NSView::displayIfNeeded and replaces it with
	 * interposed_drawIfNeeded(), which allows us to interpose and block
	 * the redrawing of plugin UIs when their redrawing behaviour
	 * is interfering with event loop behaviour.
	 */

	dispatch_once (&once_token, ^{
			Method target = class_getInstanceMethod ([NSView class], @selector(displayIfNeeded));
			original_nsview_drawIfNeeded = method_setImplementation (target, (IMP) interposed_drawIfNeeded);
		});
}

@end

#endif /* __ppc__ */

/* END OF THE PLUGIN REDRAW HACK */

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

@implementation LiveResizeNotificationObject

- (LiveResizeNotificationObject*) initWithPluginUI: (AUPluginUI*) apluginui
{
	self = [ super init ];
	if (self) {
		plugin_ui = apluginui;
	}

	return self;
}

- (void)windowWillStartLiveResizeHandler:(NSNotification*)notification
{
	plugin_ui->start_live_resize ();
}

- (void)windowWillEndLiveResizeHandler:(NSNotification*)notification
{
	plugin_ui->end_live_resize ();
}
@end

AUPluginUI::AUPluginUI (boost::shared_ptr<PluginInsert> insert)
	: PlugUIBase (insert)
	, automation_mode_label (_("Automation"))
	, preset_label (_("Presets"))
	, resizable (false)
	, req_width (0)
	, req_height (0)
	, cocoa_window (0)
	, au_view (0)
	, in_live_resize (false)
	, plugin_requested_resize (0)
	, cocoa_parent (0)
	, _notify (0)
	, _resize_notify (0)
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


	top_box.set_homogeneous (false);
	top_box.set_spacing (6);
	top_box.set_border_width (6);

	bool for_auditioner = false;
	if (insert->session().the_auditioner()) {
		for_auditioner = insert->session().the_auditioner()->the_instrument() == insert;
	}
	if (!for_auditioner) {
		add_common_widgets (&top_box);
	}

	set_spacing (0);
	pack_start (top_box, false, false);
	pack_start (low_box, true, true);

	top_box.show_all ();
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
	if (au_view) {
		low_box.signal_size_request ().connect (mem_fun (this, &AUPluginUI::lower_box_size_request));
		low_box.signal_size_allocate ().connect (mem_fun (this, &AUPluginUI::lower_box_size_allocate));
		low_box.signal_map ().connect (mem_fun (this, &AUPluginUI::lower_box_map));
		low_box.signal_unmap ().connect (mem_fun (this, &AUPluginUI::lower_box_unmap));
	}
}

AUPluginUI::~AUPluginUI ()
{
	if (_notify) {
		[[NSNotificationCenter defaultCenter] removeObserver:_notify];
	}

	if (_resize_notify) {
		[[NSNotificationCenter defaultCenter] removeObserver:_resize_notify];
	}

	NSWindow* win = get_nswindow();
	if (au_view) {
		remove_plugin_view ([[win contentView] superview]);
	}

#ifdef WITH_CARBON
	if (cocoa_parent) {
		[win removeChildWindow:cocoa_parent];
	}

	if (carbon_window) {
		/* not parented, just overlaid on top of our window */
		DisposeWindow (carbon_window);
	}
#endif

	if (editView) {
		ArdourCloseComponent (editView);
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

		DEBUG_TRACE(DEBUG::AudioUnitGUI,
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

			DEBUG_TRACE (DEBUG::AudioUnitGUI, string_compose ("the factory name is %1 bundle is %2\n",
									[factoryClassName UTF8String], CocoaViewBundlePath));

		} else {

			DEBUG_TRACE (DEBUG::AudioUnitGUI, string_compose ("No cocoaUI property cocoaViewInfo = %1\n", cocoaViewInfo));

			if (cocoaViewInfo != NULL) {
				free (cocoaViewInfo);
				cocoaViewInfo = NULL;
			}
		}
	}

	// [A] Show custom UI if view has it

	if (CocoaViewBundlePath && factoryClassName) {
		NSBundle *viewBundle	= [NSBundle bundleWithPath:[CocoaViewBundlePath path]];

		DEBUG_TRACE (DEBUG::AudioUnitGUI, string_compose ("tried to create bundle, result = %1\n", viewBundle));

		if (viewBundle == NULL) {
			error << _("AUPluginUI: error loading AU view's bundle") << endmsg;
			return -1;
		} else {
			Class factoryClass = [viewBundle classNamed:factoryClassName];
			DEBUG_TRACE (DEBUG::AudioUnitGUI, string_compose ("tried to create factory class, result = %1\n", factoryClass));
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

			DEBUG_TRACE (DEBUG::AudioUnitGUI, "got a factory instance\n");

			// make a view
			au_view = [factory uiViewForAudioUnit:*au->get_au() withSize:NSZeroSize];

			DEBUG_TRACE (DEBUG::AudioUnitGUI, string_compose ("view created @ %1\n", au_view));

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
		DEBUG_TRACE (DEBUG::AudioUnitGUI, string_compose ("Loading generic view using %1 -> %2\n", au,
								au->get_au()));
		au_view = [[AUGenericView alloc] initWithAudioUnit:*au->get_au()];
		DEBUG_TRACE (DEBUG::AudioUnitGUI, string_compose ("view created @ %1\n", au_view));
		[(AUGenericView *)au_view setShowsExpertParameters:1];
	}

	// Get the initial size of the new AU View's frame
	NSRect  frame = [au_view frame];
	req_width  = frame.size.width;
	req_height = frame.size.height;

	resizable  = [au_view autoresizingMask];

	low_box.queue_resize ();

	return 0;
}

void
AUPluginUI::update_view_size ()
{
	last_au_frame = [au_view frame];
}

bool
AUPluginUI::timer_callback ()
{
	block_plugin_redraws = 0;
#ifdef AU_DEBUG_PRINT
	std::cerr << "Resume redraws after idle\n";
#endif
	return false;
}

void
au_cf_timer_callback (CFRunLoopTimerRef timer, void* info)
{
	reinterpret_cast<AUPluginUI*> (info)->cf_timer_callback ();
}

void
AUPluginUI::cf_timer_callback ()
{
	int64_t now = PBD::get_microseconds ();

	if (!last_timer || block_plugin_redraws) {
		last_timer = now;
		return;
	}

	const int64_t usecs_slop = (1400000 / minimum_redraw_rate); // 140%

#ifdef AU_DEBUG_PRINT
	std::cerr << "Timer elapsed : " << now - last_timer << std::endl;
#endif

	if ((now - last_timer) > (usecs_slop + (1000000/minimum_redraw_rate))) {
		block_plugin_redraws = block_plugin_redraw_count;
		timer_connection.disconnect ();
		timer_connection = Glib::signal_timeout().connect (&AUPluginUI::timer_callback, 40);
#ifdef AU_DEBUG_PRINT
		std::cerr << "Timer too slow, block plugin redraws\n";
#endif
	}

	last_timer = now;
}

void
AUPluginUI::start_cf_timer ()
{
	if (!timer_needed) {
		return;
	}

	CFTimeInterval interval = 1.0 / (float) minimum_redraw_rate;

	cf_timer = CFRunLoopTimerCreate (kCFAllocatorDefault,
	                                 CFAbsoluteTimeGetCurrent() + interval,
	                                 interval, 0, 0,
	                                 au_cf_timer_callback,
	                                 0);

	CFRunLoopAddTimer (CFRunLoopGetCurrent(), cf_timer, kCFRunLoopCommonModes);
	timer_needed = false;
}

void
AUPluginUI::stop_cf_timer ()
{
	if (timer_needed) {
		return;
	}

	CFRunLoopRemoveTimer (CFRunLoopGetCurrent(), cf_timer, kCFRunLoopCommonModes);
	timer_needed = true;
	last_timer = 0;
}

void
AUPluginUI::cocoa_view_resized ()
{
	/* we can get here for two reasons:
	 *
	 * 1) the plugin window was resized by the user, a new size was
	 * allocated to the window, ::update_view_size() was called, and we
	 * explicitly/manually resized the AU NSView.
	 *
	 * 2) the plugin decided to resize itself (probably in response to user
	 * action, but not in response to an actual window resize)
	 *
	 * We only want to proceed with a window resizing in the second case.
	 */

	if (in_live_resize) {
		/* ::update_view_size() will be called at the right times and
		 * will update the view size. We don't need to anything while a
		 * live resize in underway.
		 */
		return;
	}

	if (plugin_requested_resize) {
		/* we tried to change the plugin frame from inside this method
		 * (to adjust the origin), which changes the frame of the AU
		 * NSView, resulting in a reentrant call to the FrameDidChange
		 * handler (this method). Ignore this reentrant call.
		 */
#ifdef AU_DEBUG_PRINT
		std::cerr << plugin->name() << " re-entrant call to cocoa_view_resized, ignored\n";
#endif
		return;
	}

	plugin_requested_resize = 1;

	ProcessorWindowProxy* wp = insert->window_proxy();
	if (wp) {
		/* Once a plugin has requested a resize of its own window, do
		 * NOT save the window. The user may save state with the plugin
		 * editor expanded to show "extra detail" - the plugin will not
		 * refill this space when the editor is first
		 * instantiated. Leaving the window in the "too big" state
		 * cannot be recovered from.
		 *
		 * The window will be sized to fit the plugin's own request. Done.
		 */
		wp->set_state_mask (WindowProxy::Position);
	}

	NSRect new_frame = [au_view frame];

	/* from here on, we know that we've been called because the plugin
	 * decided to change the NSView frame itself.
	 */

	/* step one: compute the change in the frame size.
	 */

	float dy = new_frame.size.height - last_au_frame.size.height;
	float dx = new_frame.size.width - last_au_frame.size.width;

	NSWindow* window = get_nswindow ();
	NSRect windowFrame= [window frame];

	/* we want the top edge of the window to remain in the same place,
	 * but the Cocoa/Quartz origin is at the lower left. So, when we make
	 * the window larger, we will move it down, which means shifting the
	 * origin toward (x,0). This will leave the top edge in the same place.
	 */

	windowFrame.origin.y    -= dy;
	windowFrame.origin.x    -= dx;
	windowFrame.size.height += dy;
	windowFrame.size.width  += dx;

	NSUInteger old_auto_resize = [au_view autoresizingMask];

	/* Some stupid AU Views change the origin of the original AU View when
	 * they are resized (I'm looking at you AUSampler). If the origin has
	 * been moved, move it back.
	 */

	if (last_au_frame.origin.x != new_frame.origin.x ||
			last_au_frame.origin.y != new_frame.origin.y) {
		new_frame.origin = last_au_frame.origin;
		[au_view setFrame:new_frame];

		NSArray* subviews = [au_view subviews];
		for (unsigned long i = 0; i < [subviews count]; ++i) {
			NSView* subview = [subviews objectAtIndex:i];
			[subview setFrame:NSMakeRect (0, 0, new_frame.size.width, new_frame.size.height)];
			break; /* only resize first subview */
		}

		/* also be sure to redraw the topbox because this can
			 also go wrong.
		 */
		top_box.queue_draw ();
	}

	/* We resize the window using Cocoa. We can't use GTK mechanisms
	 * because of this:
	 *
	 * http://www.lists.apple.com/archives/coreaudio-api/2005/Aug/msg00245.html
	 *
	 * "The host needs to be aware that changing the size of the window in
	 * response to the NSViewFrameDidChangeNotification can cause the view
	 * size to change depending on the autoresizing mask of the view. The
	 * host may need to cache the autoresizing mask of the view, set it to
	 * NSViewNotSizable, resize the window, and then reset the autoresizing
	 * mask of the view once the window has been sized."
	 *
	 */

	[au_view setAutoresizingMask:NSViewNotSizable];
	[window setFrame:windowFrame display:1];
	[au_view setAutoresizingMask:old_auto_resize];

	/* keep a copy of the size of the AU NSView. We didn't set it - the plugin did */
	last_au_frame = new_frame;
	req_width  = new_frame.size.width;
	req_height = new_frame.size.height;

	plugin_requested_resize = 0;
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
	        ArdourCloseComponent (editView);
		return -1;
	}

	if ((err = GetRootControl(carbon_window, &root_control)) != noErr) {
		error << string_compose (_("AUPlugin: cannot get root control of carbon window (err: %1)"), err) << endmsg;
		DisposeWindow (carbon_window);
	        ArdourCloseComponent (editView);
		return -1;
	}

	ControlRef viewPane;
	Float32Point location  = { 0.0, 0.0 };
	Float32Point size = { 0.0, 0.0 } ;

	if ((err = AudioUnitCarbonViewCreate (editView, *au->get_au(), carbon_window, root_control, &location, &size, &viewPane)) != noErr) {
		error << string_compose (_("AUPluginUI: cannot create carbon plugin view (err: %1)"), err) << endmsg;
		DisposeWindow (carbon_window);
	        ArdourCloseComponent (editView);
		return -1;
	}

	// resize window

	Rect bounds;
	GetControlBounds(viewPane, &bounds);
	size.x = bounds.right-bounds.left;
	size.y = bounds.bottom-bounds.top;

	req_width = (int) (size.x + 0.5);
	req_height = (int) (size.y + 0.5);

	SizeWindow (carbon_window, req_width, req_height,  true);
	low_box.set_size_request (req_width, req_height);

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
	/* despite the fact that the documentation says that [NSWindow
	   contentView] is the highest "accessible" NSView in an NSWindow, when
	   the redraw cycle is executed, displayIfNeeded is actually executed
	   on the parent of the contentView. To provide a marginal speedup when
	   checking if a given redraw is for a plugin, use this "hidden" NSView
	   to identify the plugin, so that we do not have to call [superview]
	   every time in interposed_drawIfNeeded().
	*/
	add_plugin_view ([[win contentView] superview]);

	/* this moves the AU NSView down and over to provide a left-hand margin
	 * and to clear the Ardour "task bar" (with plugin preset mgmt buttons,
	 * keyboard focus control, bypass etc).
	 */

	gint xx, yy;
	gtk_widget_translate_coordinates(
			GTK_WIDGET(low_box.gobj()),
			GTK_WIDGET(low_box.get_parent()->gobj()),
			0, 0, &xx, &yy);
	[au_view setFrame:NSMakeRect(xx, yy, req_width, req_height)];

	NSArray* subviews = [au_view subviews];
	for (unsigned long i = 0; i < [subviews count]; ++i) {
		NSView* subview = [subviews objectAtIndex:i];
		[subview setFrame:NSMakeRect (0, 0, req_width, req_height)];
		break; /* only resize first subview */
	}

	last_au_frame = [au_view frame];
	// watch for size changes of the view
	_notify = [ [NotificationObject alloc] initWithPluginUI:this andCocoaParent:NULL andTopLevelParent:win ];

	[[NSNotificationCenter defaultCenter] addObserver:_notify
		selector:@selector(auViewResized:) name:NSViewFrameDidChangeNotification
		object:au_view];

	// catch notifications that live resizing is about to start

#if HAVE_COCOA_LIVE_RESIZING
	_resize_notify = [ [ LiveResizeNotificationObject alloc] initWithPluginUI:this ];

	[[NSNotificationCenter defaultCenter] addObserver:_resize_notify
		selector:@selector(windowWillStartLiveResizeHandler:) name:NSWindowWillStartLiveResizeNotification
		object:win];

	[[NSNotificationCenter defaultCenter] addObserver:_resize_notify
		selector:@selector(windowWillEndLiveResizeHandler:) name:NSWindowDidEndLiveResizeNotification
		object:win];
#else
	/* No way before 10.6 to identify the start of a live resize (drag
	 * resize) without subclassing NSView and overriding two of its
	 * methods. Instead of that, we make the window non-resizable, thus
	 * ending confusion about whether or not resizes are plugin or user
	 * driven (they are always plugin-driven).
	 */

	Gtk::Container* toplevel = get_toplevel();
	Requisition req;

	resizable = false;

	if (toplevel && toplevel->is_toplevel()) {
		toplevel->size_request (req);
		toplevel->set_size_request (req.width, req.height);
		dynamic_cast<Gtk::Window*>(toplevel)->set_resizable (false);
	}

#endif
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
AUPluginUI::lower_box_map ()
{
	[au_view setHidden:0];
	update_view_size ();
}

void
AUPluginUI::lower_box_unmap ()
{
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
	update_view_size ();
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

void
AUPluginUI::start_live_resize ()
{
	in_live_resize = true;
}

void
AUPluginUI::end_live_resize ()
{
	in_live_resize = false;
}
