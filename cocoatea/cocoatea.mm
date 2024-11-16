/* -*- c++ -*- */

#include <AppKit/AppKit.h>

#include "view.h"


int
main (int argc, char* argv[])
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSRect rect = NSMakeRect (100, 100, 500, 500);
	NSRect frameRect = NSMakeRect (10, 10, 480, 480);

	NSWindow* win = [[NSWindow alloc] initWithContentRect:rect
	                styleMask:NSWindowStyleMaskClosable
	                backing:NSBackingStoreBuffered
	                defer:NO];

	CTView* view = [[CTView alloc] initWithFrame:frameRect];

	[win setContentView:view];
	[[NSApplication sharedApplication] activateIgnoringOtherApps : YES ];
	[win makeKeyAndOrderFront:win];

	[NSRunLoop currentRunLoop];
	[NSTimer scheduledTimerWithTimeInterval:0.1 target:view selector:@selector(timedUpdate) userInfo:nil repeats:YES];

	[[NSApplication sharedApplication] run];


	[pool release];

	return 0;
}


/*

  1, derive a new View type that will have our drawing methods
  2. add time to the run loop to trigger redraws

  figure out how to draw
  

 */
