#include <gmodule.h>
#include <gtk/gtk.h>

#include "clearlooks_style.h"
#include "clearlooks_rc_style.h"

/* declare those three functions first to avoid warnings using gcc */
GE_EXPORT void
theme_init (GTypeModule* module);

GE_EXPORT void
theme_exit (void);

GE_EXPORT GtkRcStyle*
theme_create_rc_style (void);

GE_EXPORT void
theme_init (GTypeModule* module)
{
	clearlooks_rc_style_register_type (module);
	clearlooks_style_register_type (module);
}

GE_EXPORT void
theme_exit (void)
{
}

GE_EXPORT GtkRcStyle*
theme_create_rc_style (void)
{
	return GTK_RC_STYLE (g_object_new (CLEARLOOKS_TYPE_RC_STYLE, NULL));
}
