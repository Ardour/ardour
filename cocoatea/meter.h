#pragma once

#include <AppKit/AppKit.h>

#include "view.h"

struct Meter {
	double x;
	double y;
	double width;
	double height;
	double r;
	double g;
	double b;
	double a;
	double level;
	MeterView* view;

	Meter (NSView* parent, double ax, double ay, double aw, double ah, double ar, double ag, double ab, double aa);

	void set_level (double);
	void draw (CGContextRef);
};
