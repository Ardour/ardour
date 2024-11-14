/* -*- c++ -*- */
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
	[self setNeedsDisplay:TRUE];
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
}

@end

