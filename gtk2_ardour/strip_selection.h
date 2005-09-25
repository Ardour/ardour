#ifndef __ardour_gtk_strip_selection_h__
#define __ardour_gtk_strip_selection_h__

#include <list>

class MixerStrip;

struct MixerStripSelection : list<MixerStrip*> {};

#endif /* __ardour_gtk_strip_selection_h__ */
