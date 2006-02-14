#ifndef __gtk_ardour_color_h__
#define __gtk_ardour_color_h__

#include <sigc++/signal.h>

#undef COLORID
#define COLORID(a) a,
enum ColorID {
	 #include "colors.h"
};
#undef COLORID

typedef std::map<ColorID,int> ColorMap;
extern ColorMap color_map;

extern sigc::signal<void>                  ColorsChanged;
extern sigc::signal<void,ColorID,uint32_t> ColorChanged;

#endif /* __gtk_ardour_color_h__ */
