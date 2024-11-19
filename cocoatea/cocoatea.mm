/* -*- c++ -*- */

#include <vector>

#include <AppKit/AppKit.h>

#include "meter.h"
#include "view.h"


void
layout (NSView* view)
{
}

@interface MeterWindow : NSView
{
	std::vector<Meter*> meters;
}

- (void) layout;
- (void) timedUpdate;

@end

@implementation MeterWindow

-(BOOL)isFlipped
{
  return YES;
}

-(void) layout
{
	double width = 10;
	double height = 50;
	double padding = 0;

	for (int line = 0; line < 4; ++line) {
		for (int n = 0; n < 100; ++n) {
			double r = (random() % 255) / 256.;
			double g = (random() % 255) / 256.;
			double b = (random() % 255) / 256.;
			meters.push_back (new Meter (self, padding + (n * (width + padding)), padding + (line * (height + padding)), width, height, r, g, b, 1.0));
		}
	}
}

-(void) timedUpdate
{
	for (auto & m : meters) {
		// if ((random() % 100) == 0) {
		m->set_level ((random() % (int) m->height) / m->height);
		// }
	}
}

-(void)windowWillClose:(NSNotification *)notification
{
    [NSApp terminate:self];
}

@end

int
main (int argc, char* argv[])
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSRect rect = NSMakeRect (100, 100, 1800, 399);
	NSRect frameRect = NSMakeRect (0, 0, 1800, 399);
        NSUInteger style_mask;

        style_mask = (NSTitledWindowMask |
                      NSClosableWindowMask |
                      NSMiniaturizableWindowMask |
                      NSResizableWindowMask);

	NSWindow* win = [[NSWindow alloc] initWithContentRect:rect
	                styleMask:style_mask
	                backing:NSBackingStoreBuffered
	                defer:NO];

	MeterWindow* view = [[MeterWindow alloc] initWithFrame:frameRect];

	[view layout];

	[win setContentView:view];
	[[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
	[win makeKeyAndOrderFront:win];

	[NSTimer scheduledTimerWithTimeInterval:0.02 target:view selector:@selector(timedUpdate) userInfo:nil repeats:YES];

	[[NSApplication sharedApplication] run];

	[pool release];

	return 0;
}
