/* -*- c++ -*- */

#include <vector>

#include <AppKit/AppKit.h>

#include "meter.h"
#include "view.h"

std::vector<Meter*> meters;

void
layout (NSView* view)
{
	double width = 10;
	double height = 50;
	double padding = 5;

	for (int line = 0; line < 6; ++line) {
		for (int n = 0; n < 100; ++n) {
			double r = (random() % 255) / 256.;
			double g = (random() % 255) / 256.;
			double b = (random() % 255) / 256.;
			meters.push_back (new Meter (view, n * (width + padding), line * (height + padding), width, height, r, g, b, 1.0));
		}
	}
}

int
main (int argc, char* argv[])
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSRect rect = NSMakeRect (100, 100, 1800, 399);
	NSRect frameRect = NSMakeRect (0, 0, 1800, 399);

	NSWindow* win = [[NSWindow alloc] initWithContentRect:rect
	                styleMask:NSWindowStyleMaskClosable
	                backing:NSBackingStoreBuffered
	                defer:NO];

	CTView* view = [[CTView alloc] initWithFrame:frameRect];

	layout (view);

	[win setContentView:view];
	[[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
	[win makeKeyAndOrderFront:win];

	[NSRunLoop currentRunLoop];
	[NSTimer scheduledTimerWithTimeInterval:0.02 target:view selector:@selector(timedUpdate) userInfo:nil repeats:YES];

	[[NSApplication sharedApplication] run];

	[pool release];

	return 0;
}
