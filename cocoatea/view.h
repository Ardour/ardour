#pragma once

#include <AppKit/AppKit.h>

class Meter;

@interface MeterView : NSView
{
	Meter* meter;
}

-(void)setMeter: (Meter*) meter;

@end
