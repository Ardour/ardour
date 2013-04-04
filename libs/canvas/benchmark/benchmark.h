#include "pbd/xml++.h"
#include "canvas/types.h"

extern double double_random ();
extern ArdourCanvas::Rect rect_random (double);

namespace ArdourCanvas {
	class ImageCanvas;
}

class Benchmark
{
public:
	Benchmark (std::string const &);
	virtual ~Benchmark () {}

	void set_iterations (int);
	double run ();
	
	virtual void do_run (ArdourCanvas::ImageCanvas &) = 0;
	virtual void finish (ArdourCanvas::ImageCanvas &) {}

private:
	ArdourCanvas::ImageCanvas* _canvas;
	int _iterations;
};
