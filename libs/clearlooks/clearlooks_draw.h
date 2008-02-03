#ifndef CLEARLOOKS_DRAW_H
#define CLEARLOOKS_DRAW_H

#include "clearlooks_types.h"
#include "clearlooks_style.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <cairo.h>

GE_INTERNAL void clearlooks_register_style_classic (ClearlooksStyleFunctions *functions);
GE_INTERNAL void clearlooks_register_style_glossy  (ClearlooksStyleFunctions *functions);
GE_INTERNAL void clearlooks_register_style_gummy  (ClearlooksStyleFunctions *functions);
GE_INTERNAL void clearlooks_register_style_inverted (ClearlooksStyleFunctions *functions);

#endif /* CLEARLOOKS_DRAW_H */
