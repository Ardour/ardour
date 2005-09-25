#ifndef __ardour_gtk_region_selection_h__
#define __ardour_gtk_region_selection_h__

#include <set>
#include <list>
#include <sigc++/signal_system.h>
#include <ardour/types.h>

using std::list;
using std::set;

class AudioRegionView;

struct AudioRegionComparator {
    bool operator() (const AudioRegionView* a, const AudioRegionView* b) const;
};

class AudioRegionSelection : public set<AudioRegionView*, AudioRegionComparator>, public SigC::Object
{
  public:
        AudioRegionSelection();
	AudioRegionSelection (const AudioRegionSelection&);

	AudioRegionSelection& operator= (const AudioRegionSelection&);

	void add (AudioRegionView*, bool dosort = true);
	bool remove (AudioRegionView*);
	bool contains (AudioRegionView*);

	void clear_all();
	
	jack_nframes_t start () const {
		return _current_start;
	}

	/* collides with list<>::end */

	jack_nframes_t end_frame () const { 
		return _current_end;
	}

	const list<AudioRegionView *> & by_layer() const { return _bylayer; }
	void  by_position (list<AudioRegionView*>&) const;
	
  private:
	void remove_it (AudioRegionView*);

	void add_to_layer (AudioRegionView *);
	
	jack_nframes_t _current_start;
	jack_nframes_t _current_end;

	list<AudioRegionView *> _bylayer;
};

#endif /* __ardour_gtk_region_selection_h__ */
