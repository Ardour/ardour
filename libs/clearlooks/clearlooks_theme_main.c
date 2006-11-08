#include <gmodule.h>
#include <gtk/gtk.h>

#include "clearlooks_style.h"
#include "clearlooks_rc_style.h"

G_MODULE_EXPORT void
theme_init (GTypeModule *module)
{
  clearlooks_rc_style_register_type (module);
  clearlooks_style_register_type (module);
  printf("theme_init() called from internal clearlooks engine!\n");
}

G_MODULE_EXPORT void
theme_exit (void)
{
}

G_MODULE_EXPORT GtkRcStyle *
theme_create_rc_style (void)
{
  return GTK_RC_STYLE (g_object_new (CLEARLOOKS_TYPE_RC_STYLE, NULL));  
}

/* The following function will be called by GTK+ when the module
 * is loaded and checks to see if we are compatible with the
 * version of GTK+ that loads us.
 */
G_MODULE_EXPORT const gchar* g_module_check_init (GModule *module);
const gchar*
g_module_check_init (GModule *module)
{
  return gtk_check_version (GTK_MAJOR_VERSION,
			    GTK_MINOR_VERSION,
			    GTK_MICRO_VERSION - GTK_INTERFACE_AGE);
}
