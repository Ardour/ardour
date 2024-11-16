/* -*- c++ -*- */

#include <cstdlib>
#include <stdio.h>

#include "view.h"

@implementation CTView

-(BOOL)acceptsFirstResponder
{
  printf ("acceptsFirstResponder\n");
  return YES;
}

-(void)noop: (id)sender
{
  printf ("noop\n");
}

-(BOOL)isFlipped
{
  return YES;
}

-(void) timedUpdate
{
	NSRect rect = NSMakeRect (random() % 100, random() % 100, random() % 400, random() % 400);
	[self setNeedsDisplayInRect:rect];
}

-(void)drawRect: (NSRect)rect
{
	//printf ("%g, %g %g x %g\n", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

	const NSRect *drawn_rects;
	long count;
	int i;

	[self getRectsBeingDrawn: &drawn_rects count: &count];

	NSGraphicsContext* context = [NSGraphicsContext currentContext];
	CGContextRef cg = [context CGContext];

	//printf ("%ld rects to draw\n", count);
	for (i = 0; i < count; i++) {
		//printf ("\trect %d: %g, %g %g x %g\n", i, drawn_rects[i].origin.x, drawn_rects[i].origin.y, drawn_rects[i].size.width, drawn_rects[i].size.height);
		// CGContextSetRGBFillColor (cg, 1.0, 1.0, 1.0, 0.0); 
		CGContextSetRGBStrokeColor (cg, 1.0, 1.0, 1.0, 1.0);
		// CGContextFillRect (cg, drawn_rects[i]);
		CGContextStrokeRect (cg, drawn_rects[i]);
	}

}

-(void)windowWillClose:(NSNotification *)notification
{
    [NSApp terminate:self];
}

@end

