/* gtktextbufferserialize.h
 *
 * Copyright (C) 2004 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_TEXT_BUFFER_SERIALIZE_H__
#define __GTK_TEXT_BUFFER_SERIALIZE_H__

#include <ytk/gtktextbuffer.h>

guint8 * _gtk_text_buffer_serialize_rich_text   (GtkTextBuffer     *register_buffer,
                                                 GtkTextBuffer     *content_buffer,
                                                 const GtkTextIter *start,
                                                 const GtkTextIter *end,
                                                 gsize             *length,
                                                 gpointer           user_data);

gboolean _gtk_text_buffer_deserialize_rich_text (GtkTextBuffer     *register_buffer,
                                                 GtkTextBuffer     *content_buffer,
                                                 GtkTextIter       *iter,
                                                 const guint8      *data,
                                                 gsize              length,
                                                 gboolean           create_tags,
                                                 gpointer           user_data,
                                                 GError           **error);


#endif /* __GTK_TEXT_BUFFER_SERIALIZE_H__ */
