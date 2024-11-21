/* -*- c++ -*- */
#include "meter.h"
#include "view.h"

int queued_draws = 0;

Meter::Meter (NSView* v, double ax, double ay, double aw, double ah, double ar, double ag, double ab, double aa)
	: x (ax)
	, y (ay)
	, width (aw)
	, height (ah)
	, r (ar)
	, g (ag)
	, b (ab)
	, a (aa)
	, level ((random() % 100) / 100.)
	, view (v)
	, draw_queued (true)
{
}

void
Meter::draw (CGContextRef cg, bool required)
{
	NSRect bbox = NSMakeRect (x, y, width, height);

	if (! [view needsToDrawRect:bbox]) {
		draw_queued = false;
		return;
	}

	if (!draw_queued && !required) {
		// return;
	}

	CGContextSetRGBStrokeColor (cg, 1., 1., 1., 1.);
	CGContextStrokeRect (cg, NSMakeRect (x, y, width, height));

	CGContextSetRGBFillColor (cg, r, g, b, a);
	double fill_height = height - (height * level);
	CGContextFillRect (cg, NSMakeRect (x, y + fill_height, width, height - fill_height));

	CGContextSetRGBFillColor (cg, 0, 0., 0., 1.0);
	CGContextFillRect (cg, NSMakeRect (x, y, width, fill_height));

	draw_queued = false;
}

void
Meter::set_level (double f)
{
	level = f;
	queued_draws++;
	draw_queued = true;
	[view setNeedsDisplayInRect:NSMakeRect (x, y, width, height)];
}
