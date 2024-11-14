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

-(void) timedUpdate
{
	printf ("ping\n");
	NSRect rect = NSMakeRect (random() % 100, random() % 100, random() % 400, random() % 400);
	[self setNeedsDisplayInRect:rect];
}

-(BOOL)becomeFirstResponder
{
  printf ("becomeFirstResponder\n");
  return YES;
}

-(BOOL)resignFirstResponder
{
  printf ("resignFirstResponder\n");
  return YES;
}

-(void) keyDown: (NSEvent *) theEvent
{
  printf ("keyDown\n");
  [self interpretKeyEvents: [NSArray arrayWithObject: theEvent]];
}

-(void)noop: (id)sender
{
  printf ("noop\n");
}

-(BOOL)isFlipped
{
  return YES;
}

-(void)drawRect: (NSRect)rect
{
	printf ("here we are!\n");
	printf ("%g, %g %g x %g\n", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

	const NSRect *drawn_rects;
	NSInteger count;
	int i;

	[self getRectsBeingDrawn: &drawn_rects count: &count];

	for (i = 0; i < count; i++) {
		printf ("rect %d: %g, %g %g x %g\n", i, drawn_rects[i].origin.x, drawn_rects[i].origin.y, drawn_rects[i].size.width, drawn_rects[i].size.height);
	}
}

@end

