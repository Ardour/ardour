/* -*- c++ -*- */

#include <cstdlib>
#include <vector>

#include <stdio.h>

#include "meter.h"
#include "view.h"

extern std::vector<Meter*> meters;
extern int queued_draws;

static int update_cnt = 0;

@implementation CTView

-(BOOL)isFlipped
{
  return YES;
}

-(BOOL)opaque
{
  return YES;
}

-(void) timedUpdate
{
#if 0
	assert (meters.size() >= 600);
	std::vector<int> m ({ 0, 99});
	for (auto & n : m) {
		meters[n]->set_level ((random() % (int) meters[n]->height) / meters[n]->height);
	}
#endif
#if 1
	static int howmany = 0;

	if ((update_cnt++ % 500) == 0) {
		howmany += 1;
		printf ("now drawing %d\n", howmany);
	}

	int cnt = howmany;

	while (cnt) {
		int n = random() % meters.size();
		meters[n]->set_level ((random() % (int) meters[n]->height) / meters[n]->height);
		cnt--;
}
#endif
#if 0
	for (auto & m : meters) {
		// if ((random() % 100) == 0) {
			m->set_level ((random() % (int) m->height) / m->height);
			//}
	}
#endif
}

#if 1
-(void)viewWillDraw
{
	CALayer* layer = [self layer];
	layer.contentsFormat = kCAContentsFormatRGBA8Uint;
	[super viewWillDraw];
}
#endif

-(void)drawRect: (NSRect)rect
{

	bool required_redraw;

	if (NSContainsRect (rect, [self bounds])) {
		printf ("full redraw, queued = %d\n", queued_draws);
		required_redraw = true;
	} else {
		required_redraw = false;
	}

#if 0
	if (rect.size.width != 10 && rect.size.height != 50) {
		NSRect me = [self bounds];
		printf ("%g, %g %g x %g vs %g, %g %g x %g\n",
		        rect.origin.x, rect.origin.y, rect.size.width, rect.size.height,
		        me.origin.x, me.origin.y, me.size.width, me.size.height);
	}
#endif


#if 1
	const NSRect *drawn_rects;
	long count;
	int i;

	[self getRectsBeingDrawn: &drawn_rects count: &count];

	// printf ("%ld rects to draw\n", count);

	// for (i = 0; i < count; i++) {
	// printf ("\trect %d: %g, %g %g x %g\n", i, drawn_rects[i].origin.x, drawn_rects[i].origin.y, drawn_rects[i].size.width, drawn_rects[i].size.height);
// }
#endif

	CGContextRef cg = [[NSGraphicsContext currentContext] CGContext];

	for (auto & m : meters) {
		m->draw (cg, required_redraw);
	}

	queued_draws = 0;
}

-(void)windowWillClose:(NSNotification *)notification
{
    [NSApp terminate:self];
}

@end
