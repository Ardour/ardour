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
#if 0
	int n = random() % meters.size();
	meters[n]->set_level ((random() % (int) meters[n]->height) / meters[n]->height);
#endif
	for (auto & m : meters) {
		if ((random() % 100) == 0) {
			m->set_level ((random() % (int) m->height) / m->height);
		}
	}
}

-(void)drawRect: (NSRect)rect
{
#if 0
	if (rect.size.width != 10 && rect.size.height != 50) {
		printf ("%g, %g %g x %g\n", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
	}
#endif
	CGContextRef cg = [[NSGraphicsContext currentContext] CGContext];

	for (auto & m : meters) {
		m->draw (cg);
	}

	const NSRect *drawn_rects;
	long count;
	int i;

	[self getRectsBeingDrawn: &drawn_rects count: &count];

#if 0
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

