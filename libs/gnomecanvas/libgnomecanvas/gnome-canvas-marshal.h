
#ifndef __gnome_canvas_marshal_MARSHAL_H__
#define __gnome_canvas_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:OBJECT,INT,INT,INT,INT (gnome-canvas-marshal.list:1) */
extern void gnome_canvas_marshal_VOID__OBJECT_INT_INT_INT_INT (GClosure     *closure,
                                                               GValue       *return_value,
                                                               guint         n_param_values,
                                                               const GValue *param_values,
                                                               gpointer      invocation_hint,
                                                               gpointer      marshal_data);

/* BOOLEAN:BOXED (gnome-canvas-marshal.list:2) */
extern void gnome_canvas_marshal_BOOLEAN__BOXED (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

G_END_DECLS

#endif /* __gnome_canvas_marshal_MARSHAL_H__ */

