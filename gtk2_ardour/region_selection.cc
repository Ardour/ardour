#include <algorithm>

#include <ardour/audioregion.h>

#include "regionview.h"
#include "region_selection.h"

using namespace ARDOUR;
using namespace sigc;


bool 
AudioRegionComparator::operator() (const AudioRegionView* a, const AudioRegionView* b) const
{
 	if (a == b) {
 		return false;
 	} else {
 		return a < b;
 	}
}

AudioRegionSelection::AudioRegionSelection ()
{
	_current_start = 0;
	_current_end = 0;
}

AudioRegionSelection::AudioRegionSelection (const AudioRegionSelection& other)
{

	for (AudioRegionSelection::const_iterator i = other.begin(); i != other.end(); ++i) {
		add (*i, false);
	}
	_current_start = other._current_start;
	_current_end = other._current_end;
}



AudioRegionSelection&
AudioRegionSelection::operator= (const AudioRegionSelection& other)
{
	if (this != &other) {

		clear_all();
		
		for (AudioRegionSelection::const_iterator i = other.begin(); i != other.end(); ++i) {
			add (*i, false);
		}

		_current_start = other._current_start;
		_current_end = other._current_end;
	}

	return *this;
}

void
AudioRegionSelection::clear_all()
{
	clear();
	_bylayer.clear();
}

bool AudioRegionSelection::contains (AudioRegionView* rv)
{
	if (this->find (rv) != end()) {
		return true;
	}
	else {
		return false;
	}
	
}

void
AudioRegionSelection::add (AudioRegionView* rv, bool dosort)
{
	if (this->find (rv) != end()) {
		/* we already have it */
		return;
	}

	rv->AudioRegionViewGoingAway.connect (mem_fun(*this, &AudioRegionSelection::remove_it));

	if (rv->region.first_frame() < _current_start || empty()) {
		_current_start = rv->region.first_frame();
	}
	
	if (rv->region.last_frame() > _current_end || empty()) {
		_current_end = rv->region.last_frame();
	}
	
	insert (rv);

	// add to layer sorted list
	add_to_layer (rv);
	
}

void
AudioRegionSelection::remove_it (AudioRegionView *rv)
{
	remove (rv);
}

bool
AudioRegionSelection::remove (AudioRegionView* rv)
{
	AudioRegionSelection::iterator i;

	if ((i = this->find (rv)) != end()) {

		erase (i);

		// remove from layer sorted list
		_bylayer.remove (rv);
		
		if (empty()) {

			_current_start = 0;
			_current_end = 0;

		} else {
			
			AudioRegion& region ((*i)->region);

			if (region.first_frame() == _current_start) {
				
				/* reset current start */
				
				jack_nframes_t ref = max_frames;
				
				for (i = begin (); i != end(); ++i) {
					if (region.first_frame() < ref) {
						ref = region.first_frame();
					}
				}
				
				_current_start = ref;
				
			}
			
			if (region.last_frame() == _current_end) {

				/* reset current end */
				
				jack_nframes_t ref = 0;
				
				for (i = begin (); i != end(); ++i) {
					if (region.first_frame() > ref) {
						ref = region.first_frame();
					}
				}
				
				_current_end = ref;
			}
		}

		return true;
	}

	return false;
}

void
AudioRegionSelection::add_to_layer (AudioRegionView * rv)
{
	// insert it into layer sorted position

	list<AudioRegionView*>::iterator i;

	for (i = _bylayer.begin(); i != _bylayer.end(); ++i)
	{
		if (rv->region.layer() < (*i)->region.layer()) {
			_bylayer.insert(i, rv);
			return;
		}
	}

	// insert at end if we get here
	_bylayer.insert(i, rv);
}

struct RegionSortByTime {
    bool operator() (const AudioRegionView* a, const AudioRegionView* b) {
	    return a->region.position() < b->region.position();
    }
};


void
AudioRegionSelection::by_position (list<AudioRegionView*>& foo) const
{
	list<AudioRegionView*>::const_iterator i;
	RegionSortByTime sorter;

	for (i = _bylayer.begin(); i != _bylayer.end(); ++i) {
		foo.push_back (*i);
	}

	foo.sort (sorter);
	return;
}
