/* -*- c++ -*- */
#include "meter.h"
#include "view.h"

Meter::Meter (NSView* parent, double ax, double ay, double aw, double ah, double ar, double ag, double ab, double aa)
	: x (ax)
	, y (ay)
	, width (aw)
	, height (ah)
	, r (ar)
	, g (ag)
	, b (ab)
	, a (aa)
	, level (0.)
{
	view = [[MeterView alloc] initWithFrame:NSMakeRect (x , y, width, height)];
	[view setMeter:this];
	printf ("meter view @ %g,%g %g x %g\n", x, y, width, height);
	[parent addSubview:view];
}

void
Meter::draw (CGContextRef cg)
{
	NSRect bbox = NSMakeRect (x, y, width, height);

	if (! [view needsToDrawRect:bbox]) {
		return;
	}

	CGContextSetRGBStrokeColor (cg, 1., 1., 1., 1.);
	CGContextStrokeRect (cg, NSMakeRect (x, y, width, height));

	CGContextSetRGBFillColor (cg, r, g, b, a);
	double fill_height = height - (height * level);
	CGContextFillRect (cg, NSMakeRect (x, y + fill_height, width, height - fill_height));

	CGContextSetRGBFillColor (cg, 0, 0., 0., 1.0);
	CGContextFillRect (cg, NSMakeRect (x, y, width, fill_height));
}

void
Meter::set_level (double f)
{
	level = f;
	[view setNeedsDisplayInRect:NSMakeRect (x, y, width, height)];
}
