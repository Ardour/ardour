/* -*- c++ -*- */

#include <cstdlib>
#include <vector>

#include <stdio.h>

#include "meter.h"
#include "view.h"

extern std::vector<Meter*> meters;
extern int queued_draws;

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

	if (NSContainsRect (rect, [self bounds])) {
		printf ("full redraw, queued = %d\n", queued_draws);
	}

#if 0
	if (rect.size.width != 10 && rect.size.height != 50) {
		NSRect me = [self bounds];
		printf ("%g, %g %g x %g vs %g, %g %g x %g\n",
		        rect.origin.x, rect.origin.y, rect.size.width, rect.size.height, 
		        me.origin.x, me.origin.y, me.size.width, me.size.height);
	}
#endif


#if 0
	const NSRect *drawn_rects;
	long count;
	int i;

	[self getRectsBeingDrawn: &drawn_rects count: &count];

	printf ("%ld rects to draw\n", count);

	for (i = 0; i < count; i++) {
		printf ("\trect %d: %g, %g %g x %g\n", i, drawn_rects[i].origin.x, drawn_rects[i].origin.y, drawn_rects[i].size.width, drawn_rects[i].size.height);
	}
#endif

	CGContextRef cg = [[NSGraphicsContext currentContext] CGContext];

	for (auto & m : meters) {
		m->draw (cg);
	}

	queued_draws = 0;
}

-(void)windowWillClose:(NSNotification *)notification
{
    [NSApp terminate:self];
}

@end

