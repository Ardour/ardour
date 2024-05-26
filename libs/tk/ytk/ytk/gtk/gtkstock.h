/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_STOCK_H__
#define __GTK_STOCK_H__


#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtktypeutils.h> /* for GtkTranslateFunc */

G_BEGIN_DECLS

typedef struct _GtkStockItem GtkStockItem;

struct _GtkStockItem
{
  gchar *stock_id;
  gchar *label;
  GdkModifierType modifier;
  guint keyval;
  gchar *translation_domain;
};

void     gtk_stock_add        (const GtkStockItem  *items,
                               guint                n_items);
void     gtk_stock_add_static (const GtkStockItem  *items,
                               guint                n_items);
gboolean gtk_stock_lookup     (const gchar         *stock_id,
                               GtkStockItem        *item);

/* Should free the list (and free each string in it also).
 * This function is only useful for GUI builders and such.
 */
GSList*  gtk_stock_list_ids  (void);

GtkStockItem *gtk_stock_item_copy (const GtkStockItem *item);
void          gtk_stock_item_free (GtkStockItem       *item);

void          gtk_stock_set_translate_func (const gchar      *domain,
					    GtkTranslateFunc  func,
					    gpointer          data,
					    GDestroyNotify    notify);

/* Stock IDs (not all are stock items; some are images only) */
/**
 * GTK_STOCK_ABOUT:
 *
 * The "About" item.
 *
 * Since: 2.6
 */
#define GTK_STOCK_ABOUT            "gtk-about"

/**
 * GTK_STOCK_ADD:
 *
 * The "Add" item.
 */
#define GTK_STOCK_ADD              "gtk-add"

/**
 * GTK_STOCK_APPLY:
 *
 * The "Apply" item.
 */
#define GTK_STOCK_APPLY            "gtk-apply"

/**
 * GTK_STOCK_BOLD:
 *
 * The "Bold" item.
 */
#define GTK_STOCK_BOLD             "gtk-bold"

/**
 * GTK_STOCK_CANCEL:
 *
 * The "Cancel" item.
 */
#define GTK_STOCK_CANCEL           "gtk-cancel"

/**
 * GTK_STOCK_CAPS_LOCK_WARNING:
 *
 * The "Caps Lock Warning" icon.
 *
 * Since: 2.16
 */
#define GTK_STOCK_CAPS_LOCK_WARNING "gtk-caps-lock-warning"

/**
 * GTK_STOCK_CDROM:
 *
 * The "CD-Rom" item.
 */
#define GTK_STOCK_CDROM            "gtk-cdrom"

/**
 * GTK_STOCK_CLEAR:
 *
 * The "Clear" item.
 */
#define GTK_STOCK_CLEAR            "gtk-clear"

/**
 * GTK_STOCK_CLOSE:
 *
 * The "Close" item.
 */
#define GTK_STOCK_CLOSE            "gtk-close"

/**
 * GTK_STOCK_COLOR_PICKER:
 *
 * The "Color Picker" item.
 *
 * Since: 2.2
 */
#define GTK_STOCK_COLOR_PICKER     "gtk-color-picker"

/**
 * GTK_STOCK_CONNECT:
 *
 * The "Connect" icon.
 *
 * Since: 2.6
 */
#define GTK_STOCK_CONNECT          "gtk-connect"

/**
 * GTK_STOCK_CONVERT:
 *
 * The "Convert" item.
 */
#define GTK_STOCK_CONVERT          "gtk-convert"

/**
 * GTK_STOCK_COPY:
 *
 * The "Copy" item.
 */
#define GTK_STOCK_COPY             "gtk-copy"

/**
 * GTK_STOCK_CUT:
 *
 * The "Cut" item.
 */
#define GTK_STOCK_CUT              "gtk-cut"

/**
 * GTK_STOCK_DELETE:
 *
 * The "Delete" item.
 */
#define GTK_STOCK_DELETE           "gtk-delete"

/**
 * GTK_STOCK_DIALOG_AUTHENTICATION:
 *
 * The "Authentication" item.
 *
 * Since: 2.4
 */
#define GTK_STOCK_DIALOG_AUTHENTICATION "gtk-dialog-authentication"

/**
 * GTK_STOCK_DIALOG_INFO:
 *
 * The "Information" item.
 */
#define GTK_STOCK_DIALOG_INFO      "gtk-dialog-info"

/**
 * GTK_STOCK_DIALOG_WARNING:
 *
 * The "Warning" item.
 */
#define GTK_STOCK_DIALOG_WARNING   "gtk-dialog-warning"

/**
 * GTK_STOCK_DIALOG_ERROR:
 *
 * The "Error" item.
 */
#define GTK_STOCK_DIALOG_ERROR     "gtk-dialog-error"

/**
 * GTK_STOCK_DIALOG_QUESTION:
 *
 * The "Question" item.
 */
#define GTK_STOCK_DIALOG_QUESTION  "gtk-dialog-question"

/**
 * GTK_STOCK_DIRECTORY:
 *
 * The "Directory" icon.
 *
 * Since: 2.6
 */
#define GTK_STOCK_DIRECTORY        "gtk-directory"

/**
 * GTK_STOCK_DISCARD:
 *
 * The "Discard" item.
 *
 * Since: 2.12
 */
#define GTK_STOCK_DISCARD          "gtk-discard"

/**
 * GTK_STOCK_DISCONNECT:
 *
 * The "Disconnect" icon.
 *
 * Since: 2.6
 */
#define GTK_STOCK_DISCONNECT       "gtk-disconnect"

/**
 * GTK_STOCK_DND:
 *
 * The "Drag-And-Drop" icon.
 */
#define GTK_STOCK_DND              "gtk-dnd"

/**
 * GTK_STOCK_DND_MULTIPLE:
 *
 * The "Drag-And-Drop multiple" icon.
 */
#define GTK_STOCK_DND_MULTIPLE     "gtk-dnd-multiple"

/**
 * GTK_STOCK_EDIT:
 *
 * The "Edit" item.
 *
 * Since: 2.6
 */
#define GTK_STOCK_EDIT             "gtk-edit"

/**
 * GTK_STOCK_EXECUTE:
 *
 * The "Execute" item.
 */
#define GTK_STOCK_EXECUTE          "gtk-execute"

/**
 * GTK_STOCK_FILE:
 *
 * The "File" icon.
 *
 * Since: 2.6
 */
#define GTK_STOCK_FILE             "gtk-file"

/**
 * GTK_STOCK_FIND:
 *
 * The "Find" item.
 */
#define GTK_STOCK_FIND             "gtk-find"

/**
 * GTK_STOCK_FIND_AND_REPLACE:
 *
 * The "Find and Replace" item.
 */
#define GTK_STOCK_FIND_AND_REPLACE "gtk-find-and-replace"

/**
 * GTK_STOCK_FLOPPY:
 *
 * The "Floppy" item.
 */
#define GTK_STOCK_FLOPPY           "gtk-floppy"

/**
 * GTK_STOCK_FULLSCREEN:
 *
 * The "Fullscreen" item.
 *
 * Since: 2.8
 */
#define GTK_STOCK_FULLSCREEN       "gtk-fullscreen"

/**
 * GTK_STOCK_GOTO_BOTTOM:
 *
 * The "Bottom" item.
 */
#define GTK_STOCK_GOTO_BOTTOM      "gtk-goto-bottom"

/**
 * GTK_STOCK_GOTO_FIRST:
 *
 * The "First" item.
 * RTL variant
 */
#define GTK_STOCK_GOTO_FIRST       "gtk-goto-first"

/**
 * GTK_STOCK_GOTO_LAST:
 *
 * The "Last" item.
 * RTL variant
 */
#define GTK_STOCK_GOTO_LAST        "gtk-goto-last"

/**
 * GTK_STOCK_GOTO_TOP:
 *
 * The "Top" item.
 */
#define GTK_STOCK_GOTO_TOP         "gtk-goto-top"

/**
 * GTK_STOCK_GO_BACK:
 *
 * The "Back" item.
 * RTL variant
 */
#define GTK_STOCK_GO_BACK          "gtk-go-back"

/**
 * GTK_STOCK_GO_DOWN:
 *
 * The "Down" item.
 */
#define GTK_STOCK_GO_DOWN          "gtk-go-down"

/**
 * GTK_STOCK_GO_FORWARD:
 *
 * The "Forward" item.
 * RTL variant
 */
#define GTK_STOCK_GO_FORWARD       "gtk-go-forward"

/**
 * GTK_STOCK_GO_UP:
 *
 * The "Up" item.
 */
#define GTK_STOCK_GO_UP            "gtk-go-up"

/**
 * GTK_STOCK_HARDDISK:
 *
 * The "Harddisk" item.
 *
 * Since: 2.4
 */
#define GTK_STOCK_HARDDISK         "gtk-harddisk"

/**
 * GTK_STOCK_HELP:
 *
 * The "Help" item.
 */
#define GTK_STOCK_HELP             "gtk-help"

/**
 * GTK_STOCK_HOME:
 *
 * The "Home" item.
 */
#define GTK_STOCK_HOME             "gtk-home"

/**
 * GTK_STOCK_INDEX:
 *
 * The "Index" item.
 */
#define GTK_STOCK_INDEX            "gtk-index"

/**
 * GTK_STOCK_INDENT:
 *
 * The "Indent" item.
 * RTL variant
 *
 * Since: 2.4
 */
#define GTK_STOCK_INDENT           "gtk-indent"

/**
 * GTK_STOCK_INFO:
 *
 * The "Info" item.
 *
 * Since: 2.8
 */
#define GTK_STOCK_INFO             "gtk-info"

/**
 * GTK_STOCK_ITALIC:
 *
 * The "Italic" item.
 */
#define GTK_STOCK_ITALIC           "gtk-italic"

/**
 * GTK_STOCK_JUMP_TO:
 *
 * The "Jump to" item.
 * RTL-variant
 */
#define GTK_STOCK_JUMP_TO          "gtk-jump-to"

/**
 * GTK_STOCK_JUSTIFY_CENTER:
 *
 * The "Center" item.
 */
#define GTK_STOCK_JUSTIFY_CENTER   "gtk-justify-center"

/**
 * GTK_STOCK_JUSTIFY_FILL:
 *
 * The "Fill" item.
 */
#define GTK_STOCK_JUSTIFY_FILL     "gtk-justify-fill"

/**
 * GTK_STOCK_JUSTIFY_LEFT:
 *
 * The "Left" item.
 */
#define GTK_STOCK_JUSTIFY_LEFT     "gtk-justify-left"

/**
 * GTK_STOCK_JUSTIFY_RIGHT:
 *
 * The "Right" item.
 */
#define GTK_STOCK_JUSTIFY_RIGHT    "gtk-justify-right"

/**
 * GTK_STOCK_LEAVE_FULLSCREEN:
 *
 * The "Leave Fullscreen" item.
 *
 * Since: 2.8
 */
#define GTK_STOCK_LEAVE_FULLSCREEN "gtk-leave-fullscreen"

/**
 * GTK_STOCK_MISSING_IMAGE:
 *
 * The "Missing image" icon.
 */
#define GTK_STOCK_MISSING_IMAGE    "gtk-missing-image"

/**
 * GTK_STOCK_MEDIA_FORWARD:
 *
 * The "Media Forward" item.
 * RTL variant
 *
 * Since: 2.6
 */
#define GTK_STOCK_MEDIA_FORWARD    "gtk-media-forward"

/**
 * GTK_STOCK_MEDIA_NEXT:
 *
 * The "Media Next" item.
 * RTL variant
 *
 * Since: 2.6
 */
#define GTK_STOCK_MEDIA_NEXT       "gtk-media-next"

/**
 * GTK_STOCK_MEDIA_PAUSE:
 *
 * The "Media Pause" item.
 *
 * Since: 2.6
 */
#define GTK_STOCK_MEDIA_PAUSE      "gtk-media-pause"

/**
 * GTK_STOCK_MEDIA_PLAY:
 *
 * The "Media Play" item.
 * RTL variant
 *
 * Since: 2.6
 */
#define GTK_STOCK_MEDIA_PLAY       "gtk-media-play"

/**
 * GTK_STOCK_MEDIA_PREVIOUS:
 *
 * The "Media Previous" item.
 * RTL variant
 *
 * Since: 2.6
 */
#define GTK_STOCK_MEDIA_PREVIOUS   "gtk-media-previous"

/**
 * GTK_STOCK_MEDIA_RECORD:
 *
 * The "Media Record" item.
 *
 * Since: 2.6
 */
#define GTK_STOCK_MEDIA_RECORD     "gtk-media-record"

/**
 * GTK_STOCK_MEDIA_REWIND:
 *
 * The "Media Rewind" item.
 * RTL variant
 *
 * Since: 2.6
 */
#define GTK_STOCK_MEDIA_REWIND     "gtk-media-rewind"

/**
 * GTK_STOCK_MEDIA_STOP:
 *
 * The "Media Stop" item.
 *
 * Since: 2.6
 */
#define GTK_STOCK_MEDIA_STOP       "gtk-media-stop"

/**
 * GTK_STOCK_NETWORK:
 *
 * The "Network" item.
 *
 * Since: 2.4
 */
#define GTK_STOCK_NETWORK          "gtk-network"

/**
 * GTK_STOCK_NEW:
 *
 * The "New" item.
 */
#define GTK_STOCK_NEW              "gtk-new"

/**
 * GTK_STOCK_NO:
 *
 * The "No" item.
 */
#define GTK_STOCK_NO               "gtk-no"

/**
 * GTK_STOCK_OK:
 *
 * The "OK" item.
 */
#define GTK_STOCK_OK               "gtk-ok"

/**
 * GTK_STOCK_OPEN:
 *
 * The "Open" item.
 */
#define GTK_STOCK_OPEN             "gtk-open"

/**
 * GTK_STOCK_ORIENTATION_PORTRAIT:
 *
 * The "Portrait Orientation" item.
 *
 * Since: 2.10
 */
#define GTK_STOCK_ORIENTATION_PORTRAIT "gtk-orientation-portrait"

/**
 * GTK_STOCK_ORIENTATION_LANDSCAPE:
 *
 * The "Landscape Orientation" item.
 *
 * Since: 2.10
 */
#define GTK_STOCK_ORIENTATION_LANDSCAPE "gtk-orientation-landscape"

/**
 * GTK_STOCK_ORIENTATION_REVERSE_LANDSCAPE:
 *
 * The "Reverse Landscape Orientation" item.
 *
 * Since: 2.10
 */
#define GTK_STOCK_ORIENTATION_REVERSE_LANDSCAPE "gtk-orientation-reverse-landscape"

/**
 * GTK_STOCK_ORIENTATION_REVERSE_PORTRAIT:
 *
 * The "Reverse Portrait Orientation" item.
 *
 * Since: 2.10
 */
#define GTK_STOCK_ORIENTATION_REVERSE_PORTRAIT "gtk-orientation-reverse-portrait"

/**
 * GTK_STOCK_PAGE_SETUP:
 *
 * The "Page Setup" item.
 *
 * Since: 2.14
 */
#define GTK_STOCK_PAGE_SETUP       "gtk-page-setup"

/**
 * GTK_STOCK_PASTE:
 *
 * The "Paste" item.
 */
#define GTK_STOCK_PASTE            "gtk-paste"

/**
 * GTK_STOCK_PREFERENCES:
 *
 * The "Preferences" item.
 */
#define GTK_STOCK_PREFERENCES      "gtk-preferences"

/**
 * GTK_STOCK_PRINT:
 *
 * The "Print" item.
 */
#define GTK_STOCK_PRINT            "gtk-print"

/**
 * GTK_STOCK_PRINT_ERROR:
 *
 * The "Print Error" icon.
 *
 * Since: 2.14
 */
#define GTK_STOCK_PRINT_ERROR      "gtk-print-error"

/**
 * GTK_STOCK_PRINT_PAUSED:
 *
 * The "Print Paused" icon.
 *
 * Since: 2.14
 */
#define GTK_STOCK_PRINT_PAUSED     "gtk-print-paused"

/**
 * GTK_STOCK_PRINT_PREVIEW:
 *
 * The "Print Preview" item.
 */
#define GTK_STOCK_PRINT_PREVIEW    "gtk-print-preview"

/**
 * GTK_STOCK_PRINT_REPORT:
 *
 * The "Print Report" icon.
 *
 * Since: 2.14
 */
#define GTK_STOCK_PRINT_REPORT     "gtk-print-report"


/**
 * GTK_STOCK_PRINT_WARNING:
 *
 * The "Print Warning" icon.
 *
 * Since: 2.14
 */
#define GTK_STOCK_PRINT_WARNING    "gtk-print-warning"

/**
 * GTK_STOCK_PROPERTIES:
 *
 * The "Properties" item.
 */
#define GTK_STOCK_PROPERTIES       "gtk-properties"

/**
 * GTK_STOCK_QUIT:
 *
 * The "Quit" item.
 */
#define GTK_STOCK_QUIT             "gtk-quit"

/**
 * GTK_STOCK_REDO:
 *
 * The "Redo" item.
 * RTL variant
 */
#define GTK_STOCK_REDO             "gtk-redo"

/**
 * GTK_STOCK_REFRESH:
 *
 * The "Refresh" item.
 */
#define GTK_STOCK_REFRESH          "gtk-refresh"

/**
 * GTK_STOCK_REMOVE:
 *
 * The "Remove" item.
 */
#define GTK_STOCK_REMOVE           "gtk-remove"

/**
 * GTK_STOCK_REVERT_TO_SAVED:
 *
 * The "Revert" item.
 * RTL variant
 */
#define GTK_STOCK_REVERT_TO_SAVED  "gtk-revert-to-saved"

/**
 * GTK_STOCK_SAVE:
 *
 * The "Save" item.
 */
#define GTK_STOCK_SAVE             "gtk-save"

/**
 * GTK_STOCK_SAVE_AS:
 *
 * The "Save As" item.
 */
#define GTK_STOCK_SAVE_AS          "gtk-save-as"

/**
 * GTK_STOCK_SELECT_ALL:
 *
 * The "Select All" item.
 *
 * Since: 2.10
 */
#define GTK_STOCK_SELECT_ALL       "gtk-select-all"

/**
 * GTK_STOCK_SELECT_COLOR:
 *
 * The "Color" item.
 */
#define GTK_STOCK_SELECT_COLOR     "gtk-select-color"

/**
 * GTK_STOCK_SELECT_FONT:
 *
 * The "Font" item.
 */
#define GTK_STOCK_SELECT_FONT      "gtk-select-font"

/**
 * GTK_STOCK_SORT_ASCENDING:
 *
 * The "Ascending" item.
 */
#define GTK_STOCK_SORT_ASCENDING   "gtk-sort-ascending"

/**
 * GTK_STOCK_SORT_DESCENDING:
 *
 * The "Descending" item.
 */
#define GTK_STOCK_SORT_DESCENDING  "gtk-sort-descending"

/**
 * GTK_STOCK_SPELL_CHECK:
 *
 * The "Spell Check" item.
 */
#define GTK_STOCK_SPELL_CHECK      "gtk-spell-check"

/**
 * GTK_STOCK_STOP:
 *
 * The "Stop" item.
 */
#define GTK_STOCK_STOP             "gtk-stop"

/**
 * GTK_STOCK_STRIKETHROUGH:
 *
 * The "Strikethrough" item.
 */
#define GTK_STOCK_STRIKETHROUGH    "gtk-strikethrough"

/**
 * GTK_STOCK_UNDELETE:
 *
 * The "Undelete" item.
 * RTL variant
 */
#define GTK_STOCK_UNDELETE         "gtk-undelete"

/**
 * GTK_STOCK_UNDERLINE:
 *
 * The "Underline" item.
 */
#define GTK_STOCK_UNDERLINE        "gtk-underline"

/**
 * GTK_STOCK_UNDO:
 *
 * The "Undo" item.
 * RTL variant
 */
#define GTK_STOCK_UNDO             "gtk-undo"

/**
 * GTK_STOCK_UNINDENT:
 *
 * The "Unindent" item.
 * RTL variant
 *
 * Since: 2.4
 */
#define GTK_STOCK_UNINDENT         "gtk-unindent"

/**
 * GTK_STOCK_YES:
 *
 * The "Yes" item.
 */
#define GTK_STOCK_YES              "gtk-yes"

/**
 * GTK_STOCK_ZOOM_100:
 *
 * The "Zoom 100%" item.
 */
#define GTK_STOCK_ZOOM_100         "gtk-zoom-100"

/**
 * GTK_STOCK_ZOOM_FIT:
 *
 * The "Zoom to Fit" item.
 */
#define GTK_STOCK_ZOOM_FIT         "gtk-zoom-fit"

/**
 * GTK_STOCK_ZOOM_IN:
 *
 * The "Zoom In" item.
 */
#define GTK_STOCK_ZOOM_IN          "gtk-zoom-in"

/**
 * GTK_STOCK_ZOOM_OUT:
 *
 * The "Zoom Out" item.
 */
#define GTK_STOCK_ZOOM_OUT         "gtk-zoom-out"

G_END_DECLS

#endif /* __GTK_STOCK_H__ */
