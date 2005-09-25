#include <iostream>
#include <fstream>
#include <cfloat>
#include <unistd.h>

#include <ardour/curve.h>

using namespace std;
using namespace ARDOUR;

int
curvetest (string filename)
{
	ifstream in (filename.c_str());
	stringstream line;
	Curve c (-1.0, +1.0, 0, true);
	double minx = DBL_MAX;
	double maxx = DBL_MIN;

	while (in) {
		double x, y;

		in >> x;
		in >> y;
		
		if (!in) {
			break;
		}

		if (x < minx) {
			minx = x;
		}
		
		if (x > maxx) {
			maxx = x;
		}
		
		c.add (x, y);
	}


	float foo[1024];

	c.get_vector (minx, maxx, foo, 1024);
	
	for (int i = 0; i < 1024; ++i) {
	        cout << minx + (((double) i / 1024.0) * (maxx - minx)) << ' ' << foo[i] << endl;
	}
	
	return 0;
}
