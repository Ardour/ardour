/* temporarily required due to some code design confusion (Feb 2014) */

#include "ardour/vst_types.h"

int vstfx_init (void*) { return 0; }
void vstfx_exit () {}
void vstfx_destroy_editor (VSTState*) {}
