/* -*- c++ -*- */

#include <cstdlib>
#include <vector>

#include <stdio.h>

#include "meter.h"
#include "view.h"

extern std::vector<Meter*> meters;

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

-(BOOL)opaque
{
  return NO;
}

-(void) timedUpdate
{
	for (auto & m : meters) {
		m->set_level ((random() % (int) m->height) / m->height);
	}
}

-(void)drawRect: (NSRect)rect
{
	// printf ("%g, %g %g x %g\n", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

	CGContextRef cg = [[NSGraphicsContext currentContext] CGContext];

	for (auto & m : meters) {
		m->draw (cg);
	}

#if 0
	const NSRect *drawn_rects;
	long count;
	int i;

	[self getRectsBeingDrawn: &drawn_rects count: &count];

	printf ("%ld rects to draw\n", count);
	for (i = 0; i < count; i++) {
		//printf ("\trect %d: %g, %g %g x %g\n", i, drawn_rects[i].origin.x, drawn_rects[i].origin.y, drawn_rects[i].size.width, drawn_rects[i].size.height);
		//CGContextSetRGBFillColor (cg, 1.0, 0.0, 0.0, 1.0); 
		CGContextSetRGBStrokeColor (cg, 1.0, 1.0, 1.0, 1.0);
		//CGContextFillRect (cg, drawn_rects[i]);
		CGContextStrokeRect (cg, drawn_rects[i]);
	}
#endif
}

-(void)windowWillClose:(NSNotification *)notification
{
    [NSApp terminate:self];
}

@end

