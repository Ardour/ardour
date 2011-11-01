#ifndef __gtkmm2ext_widget_state_h__
#define __gtkmm2ext_widget_state_h__

namespace Gtkmm2ext {

	/* widget states: unlike GTK, visual states like "Selected" or "Prelight"
	   are orthogonal to active states. 
	*/

	enum ActiveState {
		Active = 1,
		Mid,
	};
	
	enum VisualState {
		/* these can be OR-ed together */
		Selected = 0x1,
		Prelight = 0x2,
		Insensitive = 0x4,
	};

};

#endif /*  __gtkmm2ext_widget_state_h__ */
