/* -*- c++ -*- */

#include <cstdlib>
#include <vector>

#include <stdio.h>

#include "meter.h"
#include "view.h"

@implementation MeterView

-(void)setMeter:(Meter*)m
{
	meter = m;
}

-(BOOL)isFlipped
{
  return YES;
}

-(BOOL)opaque
{
  return NO;
}

-(void)drawRect: (NSRect)rect
{
#if 0
	// if (rect.size.width != 10 && rect.size.height != 50) {
		printf ("%g, %g %g x %g\n", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
//}
#endif
	CGContextRef cg = [[NSGraphicsContext currentContext] CGContext];
	meter->draw (cg);

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

@end

